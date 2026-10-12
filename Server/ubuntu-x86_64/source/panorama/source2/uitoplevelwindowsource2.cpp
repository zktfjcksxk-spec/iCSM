//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uitoplevelwindowsource2.h"

#include <SDL_syswm.h>
#include "renderer/uirenderengine.h"
#include "renderer/source2surface.h"
#include "uienginesdl.h"
#include "panorama/uievents.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/iinputstacksystem.h"
#include "imesource2.h"
#include "uienginesource2.h"
#include "uirenderdevicesource2.h"

#include "rendersystem/irenderdevice.h"

#include "materialsystem/imaterialsystem.h"

#include "tgaloader.h"
//#include "steam/steamvr.h"

#include "wrap_texture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifdef PANORAMA_USE_S1WRAPPER
//-----------------------------------------------------------------------------
// Game input events
//-----------------------------------------------------------------------------
enum GameInputEventType_t
{
	IE_WindowMove = IE_FirstAppEvent,
	IE_AppActivated,
};
#endif

namespace panorama
{
	uint32 GetWheelScrollLines()
	{
		return 3;
	}
}

#define LOG_SOURCE2_EVENT
//#define LOG_SOURCE2_EVENT Msg


//-----------------------------------------------------------------------------
// Purpose: Translates win32 mouse message modifiers into our modifiers
//-----------------------------------------------------------------------------
uint32 GetInputModifiers( uint unState )
{
	uint32 unModifiers = panorama::MODIFIER_NONE;
	if ( unState & IE_ControlPressed )
	{
		unModifiers |= panorama::MODIFIER_LCONTROL;
		unModifiers |= panorama::MODIFIER_RCONTROL;
	}
	if ( unState & IE_ShiftPressed )
	{
		unModifiers |= panorama::MODIFIER_LSHIFT;
		unModifiers |= panorama::MODIFIER_RSHIFT;
	}
	if ( unState & IE_AltPressed )
	{
		unModifiers |= panorama::MODIFIER_LALT;
		unModifiers |= panorama::MODIFIER_RALT;
	}
	if ( unState & IE_GuiPressed )
	{
		unModifiers |= panorama::MODIFIER_LWIN;
		unModifiers |= panorama::MODIFIER_RWIN;
	}

	return unModifiers;
}

static panorama::KeyCode SDLKeysymToKeyCode( uint keysym )
{
	switch ( keysym )
	{
	case SDLK_RETURN:                                  return panorama::KEY_ENTER;
	case SDLK_ESCAPE:                                  return panorama::KEY_ESCAPE;
	case SDLK_BACKSPACE:                               return panorama::KEY_BACKSPACE;
	case SDLK_TAB:                                     return panorama::KEY_TAB;
	case SDLK_SPACE:                                   return panorama::KEY_SPACE;
		//	case SDLK_EXCLAIM:                                 return panorama::KEY_EXCLAIM;
		//	case SDLK_QUOTEDBL:                                return panorama::KEY_QUOTEDBL;
		//	case SDLK_HASH:                                    return panorama::KEY_HASH;
		//	case SDLK_PERCENT:                                 return panorama::KEY_PERCENT;
		//	case SDLK_DOLLAR:                                  return panorama::KEY_DOLLAR;
		//	case SDLK_AMPERSAND:                               return panorama::KEY_AMPERSAND;
	case SDLK_QUOTE:                                   return panorama::KEY_APOSTROPHE;
		//	case SDLK_LEFTPAREN:                               return panorama::KEY_LPAREN;
		//	case SDLK_RIGHTPAREN:                              return panorama::KEY_RPAREN;
		//	case SDLK_ASTERISK:                                return panorama::KEY_ASTERISK;
		//	case SDLK_PLUS:                                    return panorama::KEY_PLUS;
	case SDLK_COMMA:                                   return panorama::KEY_COMMA;
	case SDLK_MINUS:                                   return panorama::KEY_MINUS;
	case SDLK_PERIOD:                                  return panorama::KEY_PERIOD;
	case SDLK_SLASH:                                   return panorama::KEY_SLASH;
	case SDLK_0:                                       return panorama::KEY_0;
	case SDLK_1:                                       return panorama::KEY_1;
	case SDLK_2:                                       return panorama::KEY_2;
	case SDLK_3:                                       return panorama::KEY_3;
	case SDLK_4:                                       return panorama::KEY_4;
	case SDLK_5:                                       return panorama::KEY_5;
	case SDLK_6:                                       return panorama::KEY_6;
	case SDLK_7:                                       return panorama::KEY_7;
	case SDLK_8:                                       return panorama::KEY_8;
	case SDLK_9:                                       return panorama::KEY_9;
		//	case SDLK_COLON:                                   return panorama::KEY_COLON;
	case SDLK_SEMICOLON:                               return panorama::KEY_SEMICOLON;
		//	case SDLK_LESS:                                    return panorama::KEY_LESS;
	case SDLK_EQUALS:                                  return panorama::KEY_EQUAL;
		//	case SDLK_GREATER:                                 return panorama::KEY_GREATER;
		//	case SDLK_QUESTION:                                return panorama::KEY_QUESTION;
		//	case SDLK_AT:                                      return panorama::KEY_AT;
	case SDLK_LEFTBRACKET:                             return panorama::KEY_LBRACKET;
	case SDLK_BACKSLASH:                               return panorama::KEY_BACKSLASH;
	case SDLK_RIGHTBRACKET:                            return panorama::KEY_RBRACKET;
		//	case SDLK_CARET:                                   return panorama::KEY_CARET;
		//	case SDLK_UNDERSCORE:                              return panorama::KEY_UNDERSCORE;
	case SDLK_BACKQUOTE:                               return panorama::KEY_BACKQUOTE;
	case SDLK_a:                                       return panorama::KEY_A;
	case SDLK_b:                                       return panorama::KEY_B;
	case SDLK_c:                                       return panorama::KEY_C;
	case SDLK_d:                                       return panorama::KEY_D;
	case SDLK_e:                                       return panorama::KEY_E;
	case SDLK_f:                                       return panorama::KEY_F;
	case SDLK_g:                                       return panorama::KEY_G;
	case SDLK_h:                                       return panorama::KEY_H;
	case SDLK_i:                                       return panorama::KEY_I;
	case SDLK_j:                                       return panorama::KEY_J;
	case SDLK_k:                                       return panorama::KEY_K;
	case SDLK_l:                                       return panorama::KEY_L;
	case SDLK_m:                                       return panorama::KEY_M;
	case SDLK_n:                                       return panorama::KEY_N;
	case SDLK_o:                                       return panorama::KEY_O;
	case SDLK_p:                                       return panorama::KEY_P;
	case SDLK_q:                                       return panorama::KEY_Q;
	case SDLK_r:                                       return panorama::KEY_R;
	case SDLK_s:                                       return panorama::KEY_S;
	case SDLK_t:                                       return panorama::KEY_T;
	case SDLK_u:                                       return panorama::KEY_U;
	case SDLK_v:                                       return panorama::KEY_V;
	case SDLK_w:                                       return panorama::KEY_W;
	case SDLK_x:                                       return panorama::KEY_X;
	case SDLK_y:                                       return panorama::KEY_Y;
	case SDLK_z:                                       return panorama::KEY_Z;
	case SDLK_CAPSLOCK:                                return panorama::KEY_CAPSLOCK;
	case SDLK_F1:                                      return panorama::KEY_F1;
	case SDLK_F2:                                      return panorama::KEY_F2;
	case SDLK_F3:                                      return panorama::KEY_F3;
	case SDLK_F4:                                      return panorama::KEY_F4;
	case SDLK_F5:                                      return panorama::KEY_F5;
	case SDLK_F6:                                      return panorama::KEY_F6;
	case SDLK_F7:                                      return panorama::KEY_F7;
	case SDLK_F8:                                      return panorama::KEY_F8;
	case SDLK_F9:                                      return panorama::KEY_F9;
	case SDLK_F10:                                     return panorama::KEY_F10;
	case SDLK_F11:                                     return panorama::KEY_F11;
	case SDLK_F12:                                     return panorama::KEY_F12;
	case SDLK_PRINTSCREEN:                             return panorama::KEY_PRINTSCREEN;
	case SDLK_SCROLLLOCK:                              return panorama::KEY_SCROLLLOCK;
	case SDLK_PAUSE:                                   return panorama::KEY_BREAK;
	case SDLK_INSERT:                                  return panorama::KEY_INSERT;
	case SDLK_HOME:                                    return panorama::KEY_HOME;
	case SDLK_PAGEUP:                                  return panorama::KEY_PAGEUP;
	case SDLK_DELETE:                                  return panorama::KEY_DELETE;
	case SDLK_END:                                     return panorama::KEY_END;
	case SDLK_PAGEDOWN:                                return panorama::KEY_PAGEDOWN;
	case SDLK_RIGHT:                                   return panorama::KEY_RIGHT;
	case SDLK_LEFT:                                    return panorama::KEY_LEFT;
	case SDLK_DOWN:                                    return panorama::KEY_DOWN;
	case SDLK_UP:                                      return panorama::KEY_UP;
	case SDLK_NUMLOCKCLEAR:                            return panorama::KEY_NUMLOCK;
	case SDLK_KP_DIVIDE:                               return panorama::KEY_PAD_DIVIDE;
	case SDLK_KP_MULTIPLY:                             return panorama::KEY_PAD_MULTIPLY;
	case SDLK_KP_MINUS:                                return panorama::KEY_PAD_MINUS;
	case SDLK_KP_PLUS:                                 return panorama::KEY_PAD_PLUS;
	case SDLK_KP_ENTER:                                return panorama::KEY_PAD_ENTER;
	case SDLK_KP_1:                                    return panorama::KEY_PAD_1;
	case SDLK_KP_2:                                    return panorama::KEY_PAD_2;
	case SDLK_KP_3:                                    return panorama::KEY_PAD_3;
	case SDLK_KP_4:                                    return panorama::KEY_PAD_4;
	case SDLK_KP_5:                                    return panorama::KEY_PAD_5;
	case SDLK_KP_6:                                    return panorama::KEY_PAD_6;
	case SDLK_KP_7:                                    return panorama::KEY_PAD_7;
	case SDLK_KP_8:                                    return panorama::KEY_PAD_8;
	case SDLK_KP_9:                                    return panorama::KEY_PAD_9;
	case SDLK_KP_0:                                    return panorama::KEY_PAD_0;
	case SDLK_KP_PERIOD:                               return panorama::KEY_PAD_DECIMAL;
		//	case SDLK_APPLICATION:                             return panorama::KEY_APPLICATION;
		//	case SDLK_POWER:                                   return panorama::KEY_POWER;
		//	case SDLK_KP_EQUALS:                               return panorama::KEY_KP_EQUALS;
	case SDLK_F13:                                     return panorama::KEY_F13;
	case SDLK_F14:                                     return panorama::KEY_F14;
	case SDLK_F15:                                     return panorama::KEY_F15;
	case SDLK_F16:                                     return panorama::KEY_F16;
	case SDLK_F17:                                     return panorama::KEY_F17;
	case SDLK_F18:                                     return panorama::KEY_F18;
	case SDLK_F19:                                     return panorama::KEY_F19;
		//	case SDLK_F20:                                     return panorama::KEY_F20;
		//	case SDLK_F21:                                     return panorama::KEY_F21;
		//	case SDLK_F22:                                     return panorama::KEY_F22;
		//	case SDLK_F23:                                     return panorama::KEY_F23;
		//	case SDLK_F24:                                     return panorama::KEY_F24;
		//	case SDLK_EXECUTE:                                 return panorama::KEY_EXECUTE;
		//	case SDLK_HELP:                                    return panorama::KEY_HELP;
		//	case SDLK_MENU:                                    return panorama::KEY_MENU;
		//	case SDLK_SELECT:                                  return panorama::KEY_SELECT;
		//	case SDLK_STOP:                                    return panorama::KEY_STOP;
		//	case SDLK_AGAIN:                                   return panorama::KEY_AGAIN;
		//	case SDLK_UNDO:                                    return panorama::KEY_UNDO;
		//	case SDLK_CUT:                                     return panorama::KEY_CUT;
		//	case SDLK_COPY:                                    return panorama::KEY_COPY;
		//	case SDLK_PASTE:                                   return panorama::KEY_PASTE;
		//	case SDLK_FIND:                                    return panorama::KEY_FIND;
		//	case SDLK_MUTE:                                    return panorama::KEY_MUTE;
	case SDLK_VOLUMEUP:                                return panorama::KEY_VOLUME_UP;
	case SDLK_VOLUMEDOWN:                              return panorama::KEY_VOLUME_DOWN;
		//	case SDLK_KP_COMMA:                                return panorama::KEY_KP_COMMA;
		//	case SDLK_KP_EQUALSAS400:                          return panorama::KEY_KP_EQUALSAS400;
		//	case SDLK_ALTERASE:                                return panorama::KEY_ALTERASE;
		//	case SDLK_SYSREQ:                                  return panorama::KEY_SYSREQ;
		//	case SDLK_CANCEL:                                  return panorama::KEY_CANCEL;
		//	case SDLK_CLEAR:                                   return panorama::KEY_CLEAR;
		//	case SDLK_PRIOR:                                   return panorama::KEY_PRIOR;
		//	case SDLK_RETURN2:                                 return panorama::KEY_RETURN2;
		//	case SDLK_SEPARATOR:                               return panorama::KEY_SEPARATOR;
		//	case SDLK_OUT:                                     return panorama::KEY_OUT;
		//	case SDLK_OPER:                                    return panorama::KEY_OPER;
		//	case SDLK_CLEARAGAIN:                              return panorama::KEY_CLEARAGAIN;
		//	case SDLK_CRSEL:                                   return panorama::KEY_CRSEL;
		//	case SDLK_EXSEL:                                   return panorama::KEY_EXSEL;
		//	case SDLK_KP_00:                                   return panorama::KEY_KP_00;
		//	case SDLK_KP_000:                                  return panorama::KEY_KP_000;
		//	case SDLK_THOUSANDSSEPARATOR:                      return panorama::KEY_THOUSANDSSEPAR;
		//	case SDLK_DECIMALSEPARATOR:                        return panorama::KEY_DECIMALSEPARAT;
		//	case SDLK_CURRENCYUNIT:                            return panorama::KEY_CURRENCYUNIT;
		//	case SDLK_CURRENCYSUBUNIT:                         return panorama::KEY_CURRENCYSUBUNI;
		//	case SDLK_KP_LEFTPAREN:                            return panorama::KEY_KP_LEFTPAREN;
		//	case SDLK_KP_RIGHTPAREN:                           return panorama::KEY_KP_RIGHTPAREN;
		//	case SDLK_KP_LEFTBRACE:                            return panorama::KEY_KP_LEFTBRACE;
		//	case SDLK_KP_RIGHTBRACE:                           return panorama::KEY_KP_RIGHTBRACE;
		//	case SDLK_KP_TAB:                                  return panorama::KEY_KP_TAB;
		//	case SDLK_KP_BACKSPACE:                            return panorama::KEY_KP_BACKSPACE;
		//	case SDLK_KP_A:                                    return panorama::KEY_KP_A;
		//	case SDLK_KP_B:                                    return panorama::KEY_KP_B;
		//	case SDLK_KP_C:                                    return panorama::KEY_KP_C;
		//	case SDLK_KP_D:                                    return panorama::KEY_KP_D;
		//	case SDLK_KP_E:                                    return panorama::KEY_KP_E;
		//	case SDLK_KP_F:                                    return panorama::KEY_KP_F;
		//	case SDLK_KP_XOR:                                  return panorama::KEY_KP_XOR;
		//	case SDLK_KP_POWER:                                return panorama::KEY_KP_POWER;
		//	case SDLK_KP_PERCENT:                              return panorama::KEY_KP_PERCENT;
		//	case SDLK_KP_LESS:                                 return panorama::KEY_KP_LESS;
		//	case SDLK_KP_GREATER:                              return panorama::KEY_KP_GREATER;
		//	case SDLK_KP_AMPERSAND:                            return panorama::KEY_KP_AMPERSAND;
		//	case SDLK_KP_DBLAMPERSAND:                         return panorama::KEY_KP_DBLAMPERSAN;
		//	case SDLK_KP_VERTICALBAR:                          return panorama::KEY_KP_VERTICALBAR;
		//	case SDLK_KP_DBLVERTICALBAR:                       return panorama::KEY_KP_DBLVERTICAL;
		//	case SDLK_KP_COLON:                                return panorama::KEY_KP_COLON;
		//	case SDLK_KP_HASH:                                 return panorama::KEY_KP_HASH;
		//	case SDLK_KP_SPACE:                                return panorama::KEY_KP_SPACE;
		//	case SDLK_KP_AT:                                   return panorama::KEY_KP_AT;
		//	case SDLK_KP_EXCLAM:                               return panorama::KEY_KP_EXCLAM;
		//	case SDLK_KP_MEMSTORE:                             return panorama::KEY_KP_MEMSTORE;
		//	case SDLK_KP_MEMRECALL:                            return panorama::KEY_KP_MEMRECALL;
		//	case SDLK_KP_MEMCLEAR:                             return panorama::KEY_KP_MEMCLEAR;
		//	case SDLK_KP_MEMADD:                               return panorama::KEY_KP_MEMADD;
		//	case SDLK_KP_MEMSUBTRACT:                          return panorama::KEY_KP_MEMSUBTRACT;
		//	case SDLK_KP_MEMMULTIPLY:                          return panorama::KEY_KP_MEMMULTIPLY;
		//	case SDLK_KP_MEMDIVIDE:                            return panorama::KEY_KP_MEMDIVIDE;
		//	case SDLK_KP_PLUSMINUS:                            return panorama::KEY_KP_PLUSMINUS;
		//	case SDLK_KP_CLEAR:                                return panorama::KEY_KP_CLEAR;
		//	case SDLK_KP_CLEARENTRY:                           return panorama::KEY_KP_CLEARENTRY;
		//	case SDLK_KP_BINARY:                               return panorama::KEY_KP_BINARY;
		//	case SDLK_KP_OCTAL:                                return panorama::KEY_KP_OCTAL;
		//	case SDLK_KP_DECIMAL:                              return panorama::KEY_KP_DECIMAL;
		//	case SDLK_KP_HEXADECIMAL:                          return panorama::KEY_KP_HEXADECIMAL;
	case SDLK_LCTRL:                                   return panorama::KEY_LCONTROL;
	case SDLK_LSHIFT:                                  return panorama::KEY_LSHIFT;
	case SDLK_LALT:                                    return panorama::KEY_LALT;
	case SDLK_LGUI:                                    return panorama::KEY_LWIN;
	case SDLK_RCTRL:                                   return panorama::KEY_RCONTROL;
	case SDLK_RSHIFT:                                  return panorama::KEY_RSHIFT;
	case SDLK_RALT:                                    return panorama::KEY_RALT;
	case SDLK_RGUI:                                    return panorama::KEY_RWIN;
		//	case SDLK_MODE:                                    return panorama::KEY_MODE;
	case SDLK_AUDIONEXT:                               return panorama::KEY_MEDIA_NEXT_TRACK;
	case SDLK_AUDIOPREV:                               return panorama::KEY_MEDIA_PREV_TRACK;
	case SDLK_AUDIOSTOP:                               return panorama::KEY_MEDIA_STOP;
	case SDLK_AUDIOPLAY:                               return panorama::KEY_MEDIA_PLAY_PAUSE;
	case SDLK_AUDIOMUTE:                               return panorama::KEY_VOLUME_MUTE;
		//	case SDLK_MEDIASELECT:                             return panorama::KEY_MEDIASELECT;
		//	case SDLK_WWW:                                     return panorama::KEY_WWW;
		//	case SDLK_MAIL:                                    return panorama::KEY_MAIL;
		//	case SDLK_CALCULATOR:                              return panorama::KEY_CALCULATOR;
		//	case SDLK_COMPUTER:                                return panorama::KEY_COMPUTER;
		//	case SDLK_AC_SEARCH:                               return panorama::KEY_AC_SEARCH;
		//	case SDLK_AC_HOME:                                 return panorama::KEY_AC_HOME;
		//	case SDLK_AC_BACK:                                 return panorama::KEY_AC_BACK;
		//	case SDLK_AC_FORWARD:                              return panorama::KEY_AC_FORWARD;
		//	case SDLK_AC_STOP:                                 return panorama::KEY_AC_STOP;
		//	case SDLK_AC_REFRESH:                              return panorama::KEY_AC_REFRESH;
		//	case SDLK_AC_BOOKMARKS:                            return panorama::KEY_AC_BOOKMARKS;
		//	case SDLK_BRIGHTNESSDOWN:                          return panorama::KEY_BRIGHTNESSDOWN;
		//	case SDLK_BRIGHTNESSUP:                            return panorama::KEY_BRIGHTNESSUP;
		//	case SDLK_DISPLAYSWITCH:                           return panorama::KEY_DISPLAYSWITCH;
		//	case SDLK_KBDILLUMTOGGLE:                          return panorama::KEY_KBDILLUMTOGGLE;
		//	case SDLK_KBDILLUMDOWN:                            return panorama::KEY_KBDILLUMDOWN;
		//	case SDLK_KBDILLUMUP:                              return panorama::KEY_KBDILLUMUP;
		//	case SDLK_EJECT:                                   return panorama::KEY_EJECT;
		//	case SDLK_SLEEP:                                   return panorama::KEY_SLEEP;
	default:                                           return panorama::KEY_NONE;

	}
}


//-----------------------------------------------------------------------------
// Purpose: Clear key repeats
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::ClearRepeats( panorama::KeyCode code )
{
	m_mapKeyRepeats.Remove( code );
}


//-----------------------------------------------------------------------------
// Purpose: Track key repeats
//-----------------------------------------------------------------------------
int panorama::CTopLevelWindowSource2::IncrementRepeats( panorama::KeyCode code )
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
panorama::CTopLevelWindowSource2::CTopLevelWindowSource2( panorama::CUIEngine *pUIEngineParent, const char *pName, InputContextHandle_t hInputContext ) :
	CTopLevelWindow( pUIEngineParent )
{
	m_hInputContext = hInputContext;
	m_p3DSurface = NULL;
	m_pRenderEngine = NULL;
	m_bFixedSurfaceSize = false;
	m_bEnforceWindowAspectRatio = false;
	m_unSurfaceWidth = 1920;
	m_unSurfaceHeight = 1080;
	m_unWindowWidth = 1920;
	m_unWindowHeight = 1080;
	m_bMouseOverWindow = false;
	m_eRenderTarget = IUIEngine::k_ERenderTargetUnset;
	m_bHidden = false;
	m_hPlatWindow = PLAT_WINDOW_INVALID;
	m_pIMEUITextField = NULL;
	m_Name = pName;
	m_bIsAnimationDisabled = false;
	m_bUseAutoMouseUpBehavior = true;
	m_bIsClosed = false;
	m_bSkipKeyCodeTypedEvent = false;
	m_bForceConsumeKBAndMouseInputEvents = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
panorama::CTopLevelWindowSource2::~CTopLevelWindowSource2()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: source2 mouse code to panorama one
//-----------------------------------------------------------------------------
panorama::MouseCode Source2MouseToPanoramaMouse( uint s2Mouse )
{
	panorama::MouseCode mouseCode = panorama::MOUSE_INVALID;
	switch ( s2Mouse )
	{
	default:
		Assert( !"Invalid source2 mouse code" );
		break;
	case MOUSE_LEFT:
		mouseCode = panorama::MOUSE_LEFT;
		break;
	case MOUSE_MIDDLE:
		mouseCode = panorama::MOUSE_MIDDLE;
		break;
	case MOUSE_RIGHT:
		mouseCode = panorama::MOUSE_RIGHT;
		break;
	case MOUSE_4:
		mouseCode = panorama::MOUSE_4;
		break;
	case MOUSE_5:
		mouseCode = panorama::MOUSE_5;
		break;
	}

	return mouseCode;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
panorama::GamePadCode Source2GamePadToPanoramaGamePad( uint s2GamePad )
{
	panorama::GamePadCode gamePadCode = panorama::XK_NULL;
	switch ( s2GamePad )
	{
	default:
		Assert( !"Invalid source2 gamepad code" );
		break;
	case KEY_XBUTTON_A:
		gamePadCode = panorama::XK_BUTTON_A;
		break;
	case KEY_XBUTTON_B:
		gamePadCode = panorama::XK_BUTTON_B;
		break;
	case KEY_XBUTTON_X:
		gamePadCode = panorama::XK_BUTTON_X;
		break;
	case KEY_XBUTTON_Y:
		gamePadCode = panorama::XK_BUTTON_Y;
		break;
	case KEY_XBUTTON_LEFT_SHOULDER:
		gamePadCode = panorama::XK_BUTTON_LEFT_SHOULDER;
		break;
	case KEY_XBUTTON_RIGHT_SHOULDER:
		gamePadCode = panorama::XK_BUTTON_RIGHT_SHOULDER;
		break;
	case KEY_XBUTTON_RTRIGGER:
		gamePadCode = panorama::XK_BUTTON_RTRIGGER;
		break;
	case KEY_XBUTTON_LTRIGGER:
		gamePadCode = panorama::XK_BUTTON_LTRIGGER;
		break;
	case KEY_XBUTTON_BACK:
		gamePadCode = panorama::XK_BUTTON_BACK;
		break;
	case KEY_XBUTTON_START:
		gamePadCode = panorama::XK_BUTTON_START;
		break;
	case KEY_XBUTTON_STICK1:
		gamePadCode = panorama::XK_BUTTON_STICK1;
		break;
	case KEY_XBUTTON_STICK2:
		gamePadCode = panorama::XK_BUTTON_STICK2;
		break;
	case KEY_XSTICK1_RIGHT:
		gamePadCode = panorama::XK_STICK1_RIGHT;
		break;
	case KEY_XSTICK1_LEFT:
		gamePadCode = panorama::XK_STICK1_LEFT;
		break;
	case KEY_XSTICK1_UP:
		gamePadCode = panorama::XK_STICK1_UP;
		break;
	case KEY_XSTICK1_DOWN:
		gamePadCode = panorama::XK_STICK1_DOWN;
		break;
	case KEY_XSTICK2_RIGHT:
		gamePadCode = panorama::XK_STICK2_RIGHT;
		break;
	case KEY_XSTICK2_LEFT:
		gamePadCode = panorama::XK_STICK2_LEFT;
		break;
	case KEY_XSTICK2_UP:
		gamePadCode = panorama::XK_STICK2_UP;
		break;
	case KEY_XSTICK2_DOWN:
		gamePadCode = panorama::XK_STICK2_DOWN;
		break;
	case KEY_XBUTTON_UP:
		gamePadCode = panorama::XK_BUTTON_UP;
	case KEY_XBUTTON_DOWN:
		gamePadCode = panorama::XK_BUTTON_DOWN;
	case KEY_XBUTTON_LEFT:
		gamePadCode = panorama::XK_BUTTON_LEFT;
	case KEY_XBUTTON_RIGHT:
		gamePadCode = panorama::XK_BUTTON_RIGHT;

	}

	return gamePadCode;
}


//-----------------------------------------------------------------------------
// Purpose: Helper function to process a keyboard input message
//-----------------------------------------------------------------------------
bool panorama::CTopLevelWindowSource2::HandleKeyboardInputMessage( panorama::InputMessage_t &input )
{
	if ( UIWindowInput()->InputEvent( input ) )
		return true;

#ifndef PANORAMA_USE_S1WRAPPER // removing - see CL comments, TODO - re-investigate input
	// If Panorama has key focus, consume this event anyways. It might be handling just the KeyTyped
	// rather than KeyCodeTyped and KeyCodeReleased
	if ( m_bAcceptKBandMouse && BIsVisible() )
	{
		panorama::IUIPanel *pFocusPanel = UIWindowInput()->GetInputFocus();
		if ( pFocusPanel && pFocusPanel->BAcceptsInput() )
		{
			return true;
		}
	}
#endif

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Run a frame, pumps input, paints, whatever.
//-----------------------------------------------------------------------------
bool panorama::CTopLevelWindowSource2::HandleInputEvent( const InputEvent_t &event )
{
	InputMessage_t input;
	panorama::KeyCode code;

	if ( m_bIsClosed )
	{
		return false;
	}
	
	switch ( event.m_nType )
	{
	case IE_Quit:
	case IE_Close:
		if ( event.m_hWnd == m_hPlatWindow )
		{
			m_bIsClosed = true;
		}
		LOG_SOURCE2_EVENT("close window\n");
		return false;
		break;
	case IE_WindowSizeChanged:
		if ( event.m_hWnd == m_hPlatWindow )
			OnWindowResize( event.m_nData, event.m_nData2 );
		Msg( "window resized\n" );
		return false;
		break;

	case IE_InputLanguageChanged:
		UIEngine()->DispatchEvent( SystemInputLanguageChanged::MakeEvent( NULL ) );
		return true;

	case IE_KeyCodeTyped:
		if ( ShouldSkipKeyCodeTypedEvent() )
		{
			SkipNextKeyCodeTypedEvent( false );
			return false;
		}
	case IE_KeyCodeReleased:

#ifdef PANORAMA_USE_S1WRAPPER
		code = ( panorama::KeyCode )event.m_nData;
#else
		code = SDLKeysymToKeyCode( event.m_nData );
#endif
		if ( code == KEY_NONE )
			break;

		input.m_eSource = k_ePanelEventSourceKeyboard;
		input.m_eInputType = ( event.m_nType == IE_KeyCodeTyped ) ? k_eKeyDown : k_eKeyUp;
		input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
		input.m_KeyData.m_KeyCode = code;
		input.m_KeyData.m_UniChar = 0;
		input.m_KeyData.m_bFirstDown = event.m_nData3 == 0;
		input.m_KeyData.m_RepeatCount = 0;
		if ( !input.m_KeyData.m_bFirstDown )
			input.m_KeyData.m_RepeatCount = IncrementRepeats( code );
		else
			ClearRepeats( code );

		input.m_KeyData.m_Modifiers = GetInputModifiers( event.m_nData2 );

		return HandleKeyboardInputMessage( input );

		break;

	case IE_KeyTyped:
		input.m_eSource = k_ePanelEventSourceKeyboard;
		input.m_eInputType = k_eKeyChar;
		input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
		input.m_KeyData.m_KeyCode = panorama::KEY_NONE;
		input.m_KeyData.m_bFirstDown = true;
		input.m_KeyData.m_RepeatCount = 0;
		input.m_KeyData.m_Modifiers = GetInputModifiers( event.m_nData2 );
		input.m_KeyData.m_UniChar = event.m_nData;
		return HandleKeyboardInputMessage( input );
		break;

	case IE_ButtonReleased:
		if ( event.m_nData >= ::MOUSE_FIRST && event.m_nData <= ::MOUSE_5 )
		{
			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = k_eMouseUp;
			input.m_MouseData.m_MouseCode = Source2MouseToPanoramaMouse( event.m_nData );
			input.m_MouseData.m_Modifiers = GetInputModifiers( event.m_nData2 );
			input.m_MouseData.m_Delta = 0;
			input.m_MouseData.m_RepeatCount = 0;
			input.m_MouseData.m_XPos = m_nMouseX;
			input.m_MouseData.m_YPos = m_nMouseY;
			return UIWindowInput()->InputEvent( input );
		}
		else if ( event.m_nData >= ::KEY_XBUTTON_A  &&  event.m_nData <= ::KEY_XSTICK2_UP )
		{
			input.m_eSource = k_ePanelEventSourceGamepad;
			input.m_eInputType = k_eGamePadUp;
			input.m_GamePadData.m_GamePadCode = Source2GamePadToPanoramaGamePad( event.m_nData );
			input.m_GamePadData.m_RepeatCount = 0;

			return  UIWindowInput()->InputEvent( input );
		}
#if defined( PANORAMA_USE_S1WRAPPER ) 
		else
		{
			// add key up handling here, (since S1 does not pass IE_KeyCodeReleased)
			code = ( panorama::KeyCode )event.m_nData2;
			
			if ( code == KEY_NONE )
				break;

			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_eInputType = k_eKeyUp;
			input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
			input.m_KeyData.m_KeyCode = code;
			input.m_KeyData.m_UniChar = 0;
			input.m_KeyData.m_bFirstDown = true;// event.m_nData3 == 0;
			input.m_KeyData.m_RepeatCount = 0;
			if ( !input.m_KeyData.m_bFirstDown )
				input.m_KeyData.m_RepeatCount = IncrementRepeats( code );
			else
				ClearRepeats( code );

			input.m_KeyData.m_Modifiers = GetInputModifiers( event.m_nData3 );

			return HandleKeyboardInputMessage( input );
		}
#endif

		break;

	case IE_ButtonPressed:
	case IE_ButtonPressedRepeating:
	case IE_ButtonDoubleClicked:
	{
		bool bHandled = false;
		if ( event.m_nData >= ::MOUSE_FIRST && event.m_nData <= ::MOUSE_5 )
		{
			EInputType eInputType = k_eMouseDown;
			if ( event.m_nType == IE_ButtonDoubleClicked )
				eInputType = k_eMouseDoubleClick;

			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = eInputType;
			input.m_MouseData.m_MouseCode = Source2MouseToPanoramaMouse( event.m_nData );
			input.m_MouseData.m_Modifiers = GetInputModifiers( event.m_nData2 );
			input.m_MouseData.m_Delta = 0;
			input.m_MouseData.m_RepeatCount = 0;
			input.m_MouseData.m_XPos = m_nMouseX;
			input.m_MouseData.m_YPos = m_nMouseY;
			
			bHandled = UIWindowInput()->InputEvent( input );

			panorama::IUIPanel *pPanelUnderMouse = UIWindowInput()->GetMouseHover();
			if ( pPanelUnderMouse )
			{
				IIMEUIObject *pIMEPanel = dynamic_cast< IIMEUIObject* > ( pPanelUnderMouse->ClientPtr() );
				if ( g_pIMEManager )
				{
					g_pIMEManager->HandleMouseDownEvent( this, pIMEPanel );
				}
			}

			// If we are clicking on a panorama panel, then accept the mouse event even we didn't actually respond to it
			if ( !bHandled && ( event.m_nData == ::MOUSE_LEFT || event.m_nData == ::MOUSE_RIGHT ) )
			{
				if ( m_bAcceptKBandMouse && BIsVisible() )
				{
					// recalculate panel under the mouse cursor. Do this first by doing a full hit test, because the hover state might not have been updated yet.
					IUIPanel *pMouseOver = GetUIRenderEngine()->HitTestCoordsAgainstLatestScreenspaceQuadCoordinates( m_nMouseX, m_nMouseY );

					// If we didn't find anything, check if we have a hover panel. This catches cases that aren't in the quad tree because we don't accept input
					// but we still want to hit test against them.  For example, when using F6 to highlight things in the debugger.
					if ( !pMouseOver && UIInputEngine()->BGetDebugHitTesting() )
					{
						pMouseOver = UIWindowInput()->GetMouseHover();						
					}
					bHandled = pMouseOver && pMouseOver->BAlwaysConsumeHoverClicks();
				}
			}

#ifndef PANORAMA_USE_S1WRAPPER
			// If we click off of panorama content, lose focus.
			if ( !bHandled )
			{
				panorama::IUIPanel *pFocusPanel = UIWindowInput()->GetInputFocus();
				if ( pFocusPanel && pFocusPanel->BCanClearFocusByClicking() )
				{
					UIWindowInput()->SetInputFocus( NULL, false, false );
				}
			}
#endif
		}
		else if ( event.m_nData >= ::KEY_XBUTTON_A  &&  event.m_nData <= ::KEY_XSTICK2_UP )
		{
			input.m_eSource = k_ePanelEventSourceGamepad;
			input.m_eInputType = k_eGamePadDown;
			input.m_GamePadData.m_GamePadCode = Source2GamePadToPanoramaGamePad( event.m_nData );
			input.m_GamePadData.m_RepeatCount = 0;

			bHandled = UIWindowInput()->InputEvent( input );
		}
		else if ( m_bAcceptKBandMouse && BIsVisible() )
		{
			// If this is a keyboard event and we have focus, mark it as handled
			/*panorama::IUIPanel *pFocusPanel = UIWindowInput()->GetInputFocus();
			bHandled = pFocusPanel && pFocusPanel->BAcceptsInput();*/

#if defined( PANORAMA_USE_S1WRAPPER )
			// TODO - sanity the above statement, commenting the code out for now
			// since implying we've handled the event doesn't necessarily mean 
			// panorama will properly handle the following KeyCodeTyped event - some 
			// other system (VGUI) may consume it first.
			// If we use the ShouldPassKeyUpToTarget pattern in engine FilterKey() so that we guarantee nothing
			// other than panorama sees the following KeyCodeTypedEvent, then it will mean we hide keys panorama isn't
			// interested in from other systems. See also HandleKeyboardInputMessages()

			// Current plan is to properly handle the event here (via HandleKeyboardInputMessages)
			// In doing so we need to disable the following KeyCodeTyped event from being handled as it will
			// effectively handle the message twice (remember that windows WM_KEYDOWN results in one IE_ButtonPressed
			// followed by at least one IE_KeyCodeTyped - If a key is held down, the IE_ButtonPressed is only seen once, and 
			// multiple IE_KeyCodeTyped events follow).

			// This way, we 

			// handle key down event here on IE_ButtonPressed instead of KeyCodeTyped
			// we need to handle this here otherwise some other system (VGUI) will 
			// handle the following KeyCodeTyped event and panorama won't get to handle it (properly) at all
			code = ( panorama::KeyCode )event.m_nData2;

			// KEY_NONE break commented out
			// We want to handle certain KEY_NONE events to prevent the console key working in textentry boxes
			// Test cases: 
			//        - typing backquote in a text entry box, 
			//        - binding non-default key to console and typing while IME candidate list showing
//			if( code == KEY_NONE )
//				break;

			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_eInputType = k_eKeyDown;
			input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
			input.m_KeyData.m_KeyCode = code;
			input.m_KeyData.m_UniChar = 0;
			input.m_KeyData.m_bFirstDown = true;// event.m_nData3 == 0;
			input.m_KeyData.m_RepeatCount = 0;
			if ( !input.m_KeyData.m_bFirstDown )
				input.m_KeyData.m_RepeatCount = IncrementRepeats( code );
			else
				ClearRepeats( code );

			input.m_KeyData.m_Modifiers = GetInputModifiers( event.m_nData3 );

			bHandled = HandleKeyboardInputMessage( input );

			// NOTE, need to ignore the next (and only the next) IE_KeyCodeTyped event
			SkipNextKeyCodeTypedEvent( true );
#endif
		}

		return bHandled;
		break;
	}

	case IE_LocateMouseClick:
	{
		m_nMouseX = event.m_nData;
		m_nMouseY = event.m_nData2;
		if ( m_hPlatWindow != PLAT_WINDOW_INVALID )
		{
			Plat_ScreenToWindowCoords( m_hPlatWindow, m_nMouseX, m_nMouseY );
			UIWindowInput()->OnMouseMove( (float)m_nMouseX, (float)m_nMouseY );
		}
		return false; // allow other layers to be informed about the mouse location, because we might be transparent to clicks at this position
	}
	break;

	case IE_AnalogValueChanged:
		if ( event.m_nData == ::MOUSE_WHEEL )
		{
			LOG_SOURCE2_EVENT( "SDL_MOUSEWHEEL\n");

			// account for windows scroll lines setting
			float delta =  (float)event.m_nData3*UIEngine()->GetWheelScrollLines();
				
			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = k_eMouseWheel;
			input.m_MouseData.m_MouseCode = panorama::MOUSE_LEFT;
			input.m_MouseData.m_Modifiers = 0; // PANORAMA_FIXME
			input.m_MouseData.m_Delta = delta;
			input.m_MouseData.m_XPos = m_nMouseX;
			input.m_MouseData.m_YPos = m_nMouseY;
			GetMouseWheelRepeats( delta < 0, abs(delta), input.m_MouseData.m_RepeatCount );
			return UIWindowInput()->InputEvent( input );
		}
		break;

#ifdef PANORAMA_USE_S1WRAPPER
	case IE_AppActivated:
#endif
	case IE_ActivateWindow:
		if ( event.m_nData )
		{
			UIWindowInput()->GotWindowFocus();
			OnGotFocus();
		}
		else
		{
			UIWindowInput()->LostWindowFocus();
			OnLostFocus();
		}
		break;

	default:
		break;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Render the window into render context
//-----------------------------------------------------------------------------



//--------------------------------------------------------------------------------------------------
// Scene View
//--------------------------------------------------------------------------------------------------

class CSceneView : public ISceneView
{
public:

	CSceneView( int n )
	{
		m_resource.m_handle = n;
		m_resource.m_nType = RESOURCE_TYPE_8CC( 's', 'w', 'p', 'c', 'h', 'a', 'i', 'n' );
	};

	virtual SwapChainHandle_t GetSwapChain() const;

	ResourceData_t m_resource;
};


SwapChainHandle_t CSceneView::GetSwapChain() const
{
	// Return swap chain 
	return &m_resource;
}

//--------------------------------------------------------------------------------------------------
// Scene Layer
//--------------------------------------------------------------------------------------------------

/// A ISceneLayer represents a rendering pass. The layer parameters will decide which objects get
/// routed to it, what render mode it uses, the rendertargets and viewports, etc.
class CSceneLayer : public ISceneLayer
{
public:
	virtual const RenderTargetDesc_t &GetRenderTargetDesc() const;

	RenderTargetDesc_t m_rtDesc;

};

const RenderTargetDesc_t &CSceneLayer::GetRenderTargetDesc() const
{
	return m_rtDesc;
}


#ifdef PLATFORM_WINDOWS
//--------------------------------------------------------------------------------------------------
// Debugger rendertarget mgmt
// 3 RTs so as to stall as little as possible
// Not released until proc. exits. If you pull up the panorama debugger the memory cost is for ever
//--------------------------------------------------------------------------------------------------

static const int DEBUGGER_RT_COUNT = 3;

static unsigned char*			s_pDebuggerImage = 0;
static int						s_nDebuggerRTIdx = 0;
static HRenderTextureStrong		s_ahDebuggerRT[ DEBUGGER_RT_COUNT ] = { RESOURCE_HANDLE_INVALID, };
static RenderTargetDesc_t		s_artDescDebugger[ DEBUGGER_RT_COUNT ];
static uint32					s_nPrevDbgH = 0, s_nPrevDbgW = 0;

static void ReleaseDebuggerRTs()
{
	if ( s_ahDebuggerRT[ 0 ].IsValid() )
	{
		for ( int i = 0; i < DEBUGGER_RT_COUNT; i++ )
		{
			s_ahDebuggerRT[ i ] = RESOURCE_HANDLE_INVALID;
		}
	}
}

static RenderTargetDesc_t GetDebuggerRT(uint32 nWd, uint32 nHt)
{
	if ( s_ahDebuggerRT[ 0 ].IsValid() && ( s_nPrevDbgW != nWd || s_nPrevDbgH != nHt ) )
	{
		s_pDebuggerImage = 0;
		ReleaseDebuggerRTs();
	}

	if ( !s_ahDebuggerRT[0].IsValid())
	{
		CTextureCreationDesc specRT;
		specRT.m_nWidth = nWd;
		specRT.m_nHeight = nHt;
		specRT.m_nNumMipLevels = 1;
		specRT.m_nDepth = 1;
		specRT.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_RENDER_TARGET_SAMPLEABLE | TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT;
		specRT.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
		specRT.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
		specRT.m_Reflectivity.Init( 1, 1, 1 );
		specRT.m_nUsage = TEXTURE_USAGE_GPU_ONLY;

		HRenderTexture dummy;
		for ( int i = 0; i < DEBUGGER_RT_COUNT; i++ )
		{
			s_ahDebuggerRT[ i ] = g_pRenderDevice->FindOrCreateTexture( "panorama_debugger_rt_backbuff0.vtex", true, &specRT );
			s_artDescDebugger[ i ] = RenderTargetDesc_t( s_ahDebuggerRT[ i ], dummy, dummy, RENDER_SRGB );
		}

		s_nPrevDbgW = nWd;
		s_nPrevDbgH = nHt;
	}

	int idx = s_nDebuggerRTIdx;
	s_nDebuggerRTIdx = ( s_nDebuggerRTIdx + 1 ) % DEBUGGER_RT_COUNT;
	return s_artDescDebugger[idx];
}

void panorama::CTopLevelWindowSource2::CopyDebuggerRtBits( uint32* pBitsOut, uint32 nWd, uint32 nHt )
{
	if ( !s_ahDebuggerRT[0].IsValid() ) return;

	if ( !s_pDebuggerImage )
	{
		memset( pBitsOut, 0xFF, nWd*nHt * 4 );
		s_pDebuggerImage = (unsigned char*)pBitsOut;
	}
}
#endif

//--------------------------------------------------------------------------------------------------
// RenderWindow (src1)
//--------------------------------------------------------------------------------------------------

void panorama::CTopLevelWindowSource2::RenderWindow( PlatWindow_t hwnd, bool bDebugger )
{
#ifdef PLATFORM_WINDOWS
	if ( bDebugger )
	{
		SetUseAutoMouseUpBehavior( false );			// Since the debugger window is unknown to g_inputsys
													// we need to not check that g_inputSys thinks it's down
	}
#endif

	CRenderContext s2renderContext( g_pMaterialSystem );

#ifdef PLATFORM_WINDOWS
	if ( bDebugger )
	{
		RenderTargetDesc_t rtDesc = GetDebuggerRT(m_unWindowWidth, m_unWindowHeight);
		s2renderContext.PushDebuggerRenderTarget( rtDesc );
	}
#endif
	RenderTargetDesc_t rtDesc;
	rtDesc.m_pColorTargets[ 0 ] = HRenderTexture( &CRenderContext::m_backBufferResourceData );
	rtDesc.m_nColorTargetCount = -1;
	
	CSceneLayer layer;
	layer.m_rtDesc = rtDesc;

	CSceneView view( bDebugger );
	m_hPlatWindow = hwnd;

	// Need to render now... this is called by worker thread in source2, and we should just do everything
	// in the panaorama UIRenderEngine::RenderThread here and not create a thread for it of our own.  I think...
	m_pRenderEngine->RunRenderThreadFrame( &view, &s2renderContext, &layer, !m_bIsAnimationDisabled );

#ifdef PLATFORM_WINDOWS

	if ( bDebugger )
	{
		s2renderContext.PopDebuggerRenderTarget();
		InvalidateRect( (HWND)hwnd, NULL, TRUE );

		S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)s_ahDebuggerRT[ (s_nDebuggerRTIdx + 1) % 3 ].GetResourceHandle()->m_handle;

		ITexture* pTex = pTexture->GetS1Texture();

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

		if ( (s_pDebuggerImage != NULL) && (pTex != NULL) && BIsVisible())
		{
			if ( g_pMaterialSystem->CanDownloadTextures() )
			{
				pRenderContext->ReadPixels( 0, 0, m_unWindowWidth, m_unWindowHeight, s_pDebuggerImage, IMAGE_FORMAT_RGBA8888, pTex );
			}
		}
	}

#endif
}

//-----------------------------------------------------------------------------
// Purpose: Set window position
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::SetWindowPosition( float x, float y )
{
	// Not supported?
	AssertMsg( false, "SetWindowPosition does nothing in source2" );
}


//-----------------------------------------------------------------------------
// Purpose: Get window position and size
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::GetWindowBounds( float &left, float &top, float &right, float &bottom )
{
	// Not supported?
	AssertMsg( false, "GetWindowBounds does nothing in source2" );

	left = m_unXPos;
	top = m_unYPos;
	right = left + m_unWindowWidth;
	bottom = top + m_unWindowHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Get position and size of window's client area
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::GetClientDimensions( float &width, float &height )
{
	width = m_unWindowWidth;
	height = m_unWindowHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Get window position
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::GetWindowPosition( float &x, float &y )
{
	x = m_unXPos;
	y = m_unYPos;
}


//-----------------------------------------------------------------------------
// Purpose: Activates window, bringing to foreground
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::Activate( bool bForceful )
{
	if ( m_hPlatWindow == PLAT_WINDOW_INVALID )
		return;

#ifdef PLATFORM_WINDOWS_PC

	HWND hWnd = (HWND)Plat_WindowToOsSpecificHandle( m_hPlatWindow );

	// SetWindowPos/ShowWindow must happen before SetForegroundWindow
	// NOTE: SetWindowPos / ShowWindow before SetForegroundWindow/SetFocus
	// more closely resembles the order of messages when activating a fullscreen app
	// that is minimized by alt-tab.  When ShowWindow was before SetWindowPos the game would not
	// correctly return to the fullscreen state
	::SetWindowPos( hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
	if ( ::IsIconic( hWnd ) )
	{
		::ShowWindow( hWnd, SW_RESTORE );
	}

	// SendInput with a null input event appears to allow SetForegroundWindow to steal focus
	// on any version of Windows. Weird but true. I suppose the internal Windows assumption
	// is that there is a small window of time where apps may take focus in response to input?
	if ( bForceful )
	{
		INPUT in = {}; // zero-init, defaults to an empty mouseinput struct
		::SendInput( 1, &in, sizeof( in ) );
	}
	::SetForegroundWindow( hWnd );
	::SetFocus( hWnd );

#else

#ifndef PANORAMA_USE_S1WRAPPER
	Plat_SetActiveWindow( m_hPlatWindow );
#endif

#endif
}


//-----------------------------------------------------------------------------
// Purpose: Shutdown/close this window
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::Shutdown()
{
	if ( g_pIMEManager )
	{
		g_pIMEManager->SetActiveUIView( this, false );
	}

	CTopLevelWindow::Shutdown();

	if ( m_pRenderEngine )
	{
		delete m_pRenderEngine;
		m_pRenderEngine = NULL;
	}
	
	// unlike the D3D implementation, we must destroy the surface before the window or it winds up trying to destroy resources on the wrong context
	if ( m_p3DSurface )
	{
		delete m_p3DSurface;
		m_p3DSurface = NULL;
	}

	delete m_pIMEUITextField;
	m_pIMEUITextField = NULL;

#ifdef PLATFORM_WINDOWS
	ReleaseDebuggerRTs();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the surface, creating a window and D3D device
//-----------------------------------------------------------------------------
bool panorama::CTopLevelWindowSource2::BInitializeSurface( int xPos, int yPos, int nWidth, int nHeight, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, bool bAcceptKBandMouse, bool bDrawToBackBuffer )
{
	m_bFixedSurfaceSize = bFixedSurfaceSize;
	m_bEnforceWindowAspectRatio = bEnforceWindowAspectRatio;
	m_bUseCustomMouseCursor = bUseCustomMouseCursor;
	m_bAcceptKBandMouse = bAcceptKBandMouse;
	m_unWindowWidth = nWidth;
	m_unWindowHeight = nHeight;
	m_unSurfaceWidth = nWidth;
	m_unSurfaceHeight = nHeight;
	m_unXPos = xPos;
	m_unYPos = yPos;

	// Now initialize the 3d surface
	CSource2Surface *pSurface = new CSource2Surface( m_Name.Get() );
	if ( !pSurface->BInitialize( m_unSurfaceWidth, m_unSurfaceHeight, m_unWindowWidth, m_unWindowHeight, bEnforceWindowAspectRatio, bFixedSurfaceSize, m_pCursorRender /*, bDrawToBackBuffer */ ) )
	{
		delete pSurface;
		// TODO need more cleanup than this!
		return false;
	}

	m_pSurfaceInterface = pSurface;
	m_pSurfaceInterface->SetWindowScaleFactor( GetWindowScaleFactor() );

	m_p3DSurface = pSurface;
	m_pRenderDevice = ( ( CUIEngineSource2 * )UIEngine() )->GetRenderDevice();
	m_pRenderEngine = new CUIRenderEngine( m_pUIEngineParent, m_p3DSurface, m_pInputEngine, this, m_unSurfaceWidth, m_unSurfaceHeight );
	
	if ( g_pIMEManager && g_pIMEManager->IsValid() && !m_pIMEUITextField )
	{
		m_pIMEUITextField = new CIMEUITextField();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Resize 3D surface
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::OnWindowResize( uint32 width, uint32 height )
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
				m_listVisiblePanels[ i ]->InvalidateSizeAndPosition();
			}

			FOR_EACH_LL( m_listInvisiblePanels, i )
			{
				m_listInvisiblePanels[ i ]->InvalidateSizeAndPosition();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the mouse is moving over our window
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::OnMouseEnter()
{
	if ( m_bMouseOverWindow )
		return;
	
	m_bMouseOverWindow = true;
}


//-----------------------------------------------------------------------------
// Purpose: set the mouse cursor to this cursor please
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::SetMouseCursor( EMouseCursors eCursor )
{
#ifndef PANORAMA_USE_S1WRAPPER
	// We can only avoid duplicate sets if we have an input context.  If
	// we're using the raw inputsystem any other code in the process
	// could have changed the cursor so we can't assume our current
	// value is accurate.
	if ( m_hInputContext && eCursor == m_eCursorCurrent )
		return;
#endif

	VPROF_BUDGET_DETAILED( "panorama::CTopLevelWindowSource2::SDL::SetMouseCursor", VPROF_BUDGETGROUP_TENFOOT );
	m_eCursorCurrent = eCursor; // store off the current cursor, the renderer may want to use it

	if ( m_bUseCustomMouseCursor )
		return; // manually pushed by the renderer instead of windows

	InputStandardCursor_t eStdCursor;
	switch( eCursor )
	{
	case eMouseCursor_None:
		eStdCursor = INPUT_CURSOR_NONE;
		break;
	case eMouseCursor_Arrow:
		eStdCursor = INPUT_CURSOR_ARROW;
		break;
	case eMouseCursor_IBeam:
		eStdCursor = INPUT_CURSOR_IBEAM;
		break;
	case eMouseCursor_SizeWE:
		eStdCursor = INPUT_CURSOR_SIZE_W_E;
		break;
	case eMouseCursor_SizeNS:
		eStdCursor = INPUT_CURSOR_SIZE_N_S;
		break;
	case eMouseCursor_Hand:
		eStdCursor = INPUT_CURSOR_HAND;
		break;
	case eMouseCursor_Hand_Closed:
		eStdCursor = INPUT_CURSOR_HAND; // PANORAMA_USE_S1WRAPPER
		break;
#ifndef PANORAMA_USE_S1WRAPPER
	case eMouseCursor_PassThrough:
		eStdCursor = INPUT_CURSOR_PASS_THROUGH;
		break;
#endif
	default:
		AssertMsg1( false, "Missing panorama to source engine mapping for cursor %d", eCursor );
		eStdCursor = INPUT_CURSOR_ARROW;
		break;
	}

#ifdef PANORAMA_USE_S1WRAPPER
	InputCursorHandle_t hCursor = g_pInputSystem->GetStandardCursor( eStdCursor );
	if ( m_hInputContext )
	{
		g_pInputStackSystem->SetCursorIcon( m_hInputContext, hCursor );
	}
	else
	{
		g_pInputSystem->SetCursorIcon( hCursor );
	}
#else
	if ( m_hInputContext )
	{
		InputCursorHandle_t hCursor = g_pInputStackSystem->GetStandardCursor( m_hInputContext, eStdCursor );
		g_pInputStackSystem->SetCursorIcon( m_hInputContext, hCursor );
	}
	// FIXME - Not having an input context is problematic since this cursor
	// set will compete with all other code doing cursor sets and probably
	// won't last.  However panorama panel world scene objects currently
	// do not have an input context so we need to do something sensible for
	// that case and can't just error.
	// Additionally the panorama debugger doesn't have an input context
	// and will hit this path for mousing over the debugging.
	else if ( eStdCursor != INPUT_CURSOR_PASS_THROUGH )
	{
		InputCursorHandle_t hCursor = g_pInputSystem->GetStandardCursor( eStdCursor );
		g_pInputSystem->SetCursorIcon( hCursor );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: return true if this window has key focus
//-----------------------------------------------------------------------------
bool panorama::CTopLevelWindowSource2::BHasFocus()
{
	if ( m_hPlatWindow == PLAT_WINDOW_INVALID )
		return false;

	return Plat_IsWindowFocused( m_hPlatWindow );
}


//-----------------------------------------------------------------------------
// Purpose: return true any part of the window is being rendered
//-----------------------------------------------------------------------------
bool panorama::CTopLevelWindowSource2::BIsVisible()
{
	if ( !m_p3DSurface )
		return false;
	
	if ( m_bHidden )
		return false;

	if ( m_p3DSurface->BSurfaceOccluded() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void* panorama::CTopLevelWindowSource2::GetNativeWindowHandle()
{
	// bugbug jmc - delete this function completely from all window implementations, should never
	// have existed and is a total violation of the abstraction the framework should provide
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: force back topmost
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::SetPreventForceWindowOnTop( bool bPreventForceTopLevel )
{
	panorama::CTopLevelWindow::SetPreventForceWindowOnTop( bPreventForceTopLevel );
}


//-----------------------------------------------------------------------------
// Purpose: user tabbed away
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::OnLostFocus()
{
	if ( g_pIMEManager )
	{
		g_pIMEManager->SetActiveUIView( this, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: got focus back
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::OnGotFocus()
{
	if ( g_pIMEManager )
	{
		g_pIMEManager->SetActiveUIView( this, true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Mark whether this window is visible or not
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::SetVisible( bool bVisible )
{
	if ( m_bHidden == !bVisible )
		return;

	m_bHidden = !bVisible;

	if ( m_bHidden )
	{
		FOR_EACH_LL( m_listInvisiblePanels, i )
		{
			m_listInvisiblePanels[i]->SetReadyForDisplay( false );
		}

		FOR_EACH_LL( m_listVisiblePanels, i )
		{
			m_listVisiblePanels[i]->SetReadyForDisplay( false );
		}
	}
	else
	{
		FOR_EACH_LL( m_listInvisiblePanels, i )
		{
			m_listInvisiblePanels[i]->MarkStylesDirty( true );
			m_listInvisiblePanels[i]->SetReadyForDisplay( true );
		}

		FOR_EACH_LL( m_listVisiblePanels, i )
		{
			m_listVisiblePanels[i]->MarkStylesDirty( true );
			m_listVisiblePanels[i]->SetReadyForDisplay( true );
		}
	}

	panorama::DispatchEvent( TopLevelWindowVisibilityChanged(), ( panorama::IUIPanel *) nullptr, this );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	// ValidatePtr( (CD3D10D2DSurface *)m_p3DSurface );
	panorama::CTopLevelWindow::Validate( validator, pchName );
}

//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::ValidateStatics( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE_STATIC( "panorama::CTopLevelWindowSource2" );
	ValidateObj( s_vecpWindowInstances );
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
IIMEUIObject *panorama::CTopLevelWindowSource2::GetFocusedObject()
{
	return m_pIMEUITextField ? m_pIMEUITextField->GetFocusedTextEntry() : NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
PlatWindow_t panorama::CTopLevelWindowSource2::GetAssociatedPlatWindow()
{
	return m_pIMEUITextField ? m_hPlatWindow : PLAT_WINDOW_INVALID;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void panorama::CTopLevelWindowSource2::TextEntryFocusChange( IUIPanel *pPanel )
{
	CTextEntry *pTextEntry = pPanel ? (CTextEntry*)ToPanel2D( pPanel ) : NULL;

	if ( !m_pIMEUITextField )
	{
		// We can use the SDL IME support on non-Windows platforms,
		// so if we've gained focus give a hint about where the
		// IME UI should go.
#if defined( PLATFORM_LINUX ) || defined( PLATFORM_OSX )
		if ( pTextEntry && g_pInputSystem )
		{
			float xPos = 0.0f;
			float yPos = 0.0f;
			pTextEntry->GetPositionWithinAncestor( NULL, &xPos, &yPos );
			// Don't give any width but give the appropriate height.
			// This appears to cause the IME UI to show up just
			// below the textentry and aligned with the left side
			// of the textentry.  We could try and position at
			// the caret coordinates but having the IME UI bounce
			// around as the caret moves might be undesirable.
			g_pInputSystem->SetIMETextLocation( (int)xPos, (int)yPos, 0, (int)pPanel->GetActualLayoutHeight() );
		}
		if ( (!pTextEntry || !pTextEntry->BHasKeyFocus()) &&
			g_pInputSystem )
		{
			g_pInputSystem->DismissIME();
		}
#endif

		// not setup for panorama IME at all
		return;
	}

	// only set/cleared once regardless of noisy external focus events
	if ( pTextEntry && pTextEntry->BHasKeyFocus() && m_pIMEUITextField->GetFocusedTextEntry() != pTextEntry )
	{
		// Gained key focus
		Log_Detailed( g_pIMEManager->GetLoggingChannel(), Color( 255, 127, 0 ), "TextEntryFocusChange(): Set for %s\n", pPanel->BHasID() ? pPanel->GetID() : CFmtStr( "%p", pPanel ).Get() );

		if ( g_pIMEManager->GetActiveUIView() != this )
		{
			// have to compensate for lack of initial app activation event (or occurred early before IME manager established)
			// as soon as key focus established, app by definition should have had a window activation
			g_pIMEManager->SetActiveUIView( this, true );
		}

		m_pIMEUITextField->SetFocusedTextEntry( pTextEntry );
	}
	else if ( !pTextEntry || ( !pTextEntry->BHasKeyFocus() && m_pIMEUITextField->GetFocusedTextEntry() == pTextEntry ) )
	{
		// Lost key focus
		Log_Detailed( g_pIMEManager->GetLoggingChannel(), Color( 255, 127, 0 ), "TextEntryFocusChange(): Cleared for %s\n", (pPanel && pPanel->BHasID()) ? pPanel->GetID() : CFmtStr( "%p", pPanel ).Get() );

		m_pIMEUITextField->SetFocusedTextEntry( NULL );
	}
}

void panorama::CTopLevelWindowSource2::TextEntryInvalid( IUIPanel *pPanel )
{
	if ( !m_pIMEUITextField )
	{
		// not setup for IME at all
		return;
	}

	CTextEntry *pTextEntry = pPanel ? (CTextEntry*)ToPanel2D( pPanel ) : NULL;

	if ( m_pIMEUITextField->GetFocusedTextEntry() == pTextEntry ) 
	{
		Log_Detailed( g_pIMEManager->GetLoggingChannel(), Color( 255, 127, 0 ), "TextEntryInvalid(): Cleared for %s\n", pPanel->BHasID() ? pPanel->GetID() : CFmtStr( "%p", pPanel ).Get() );

		// The text entry has been deleted or invalidated, abandon any further IME traffic to it
		m_pIMEUITextField->ClearFocusedTextEntry( pTextEntry );
	}
}


panorama::CImageResourceManager* panorama::CTopLevelWindowSource2::AccessImageManager()
{
	return ( ( CUIEngineSource2 * )UIEngine() )->GetImageResourceManager();
}


void panorama::CTopLevelWindowSource2::SetAnimationDisabled( bool bDisableAnimation )
{
	m_bIsAnimationDisabled = bDisableAnimation;
}


void panorama::CTopLevelWindowSource2::SetForceConsumeKBAndMouseInputEvents( bool bForce )
{
	m_bForceConsumeKBAndMouseInputEvents = bForce;
}


bool panorama::CTopLevelWindowSource2::BForceConsumeKBAndMouseInputEvents()
{
	return m_bForceConsumeKBAndMouseInputEvents;
}