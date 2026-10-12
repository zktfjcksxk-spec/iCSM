//===== Copyright © 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef IMEMANAGER_H
#define IMEMANAGER_H

#pragma once

#include "iimemanager.h"
#include "tier2/tier2.h"

extern bool g_bIMEDetailedLogging;
#ifndef Log_Detailed
#define Log_Detailed( Channel, /* [LoggingMetaData_t *], [Color], Message, */ ... ) do { if (g_bIMEDetailedLogging) InternalMsg( Channel, LS_MESSAGE, /* [Color], Message, */ ##__VA_ARGS__ );  } while( 0 )
#endif

DECLARE_LOGGING_CHANNEL( LOG_IME );
#define LOG_COLOR_GREEN Color( 0, 255, 0, 255 )
#define LOG_COLOR_CYAN Color( 0, 255, 255, 255 )
#define LOG_COLOR_YELLOW Color( 255, 255, 0, 255 )


//DISABLED_FOR_S2

#define MAX_UINTP           ((uintp)~0)

#define     NOTE_UNUSED2(a,b)       (a);(b)
#define     NOTE_UNUSED3(a,b,c)     (a);(b);(c)
#define     NOTE_UNUSED4(a,b,c,d)   (a);(b);(c);(d)

#ifdef COMPILER_MSVC
// turn off a couple warnings in third party code
#pragma warning(disable : 4456) // local hides local
#endif

class GFxIMEManagerWin32;
class IIMEUIView;
class IIMEUIObject;
class IIMEUITextField;

enum IMEEventResult
{
    IME_EVENT_NOTHANDLED      = 0x00,
    IME_EVENT_HANDLED         = 0x01,
    IME_EVENT_NODEFAULTACTION = 0x02,	// Caller should do no further processing, IME manager wants to absorb it
    IME_EVENT_COMPLETED       = (IME_EVENT_HANDLED | IME_EVENT_NODEFAULTACTION)
};

class Value
{
};

class FunctionHandler
{
public:
    // Parameters passed in to the callback from the VM
    struct Params 
    {
        Value*          pRetVal;
        IIMEUIView*		pUIView;
        Value*          pThis;
        Value*          pArgsWithThisRef;
        Value*          pArgs;
        unsigned        ArgCount;
        void*           pUserData;
    };

    virtual ~FunctionHandler() {}

    virtual void Call(const Params& params) = 0;  
};

class DisplayObject
{
};

class ASString
{
};

class KeyModifiers 
{
public:
    enum
    {
        Key_ShiftPressed    = 0x01,
        Key_CtrlPressed     = 0x02,
        Key_AltPressed      = 0x04,
        Key_CapsToggled     = 0x08,
        Key_NumToggled      = 0x10,
        Key_ScrollToggled   = 0x20,
        Key_ExtendedKey     = 0x40, // set when right shift, alt or ctrl is pressed

        Initialized_Bit     = 0x80,
        Initialized_Mask    = 0xFF
    };
    uint8   States;

    KeyModifiers() : States(0) { }
    KeyModifiers(uint8 st) : States(uint8(st | Initialized_Bit)) { }

    void Reset() { States = 0; }

    bool IsShiftPressed() const { return (States & Key_ShiftPressed) != 0; }
    bool IsCtrlPressed() const  { return (States & Key_CtrlPressed) != 0; }
    bool IsAltPressed() const   { return (States & Key_AltPressed) != 0; }
    bool IsCapsToggled() const  { return (States & Key_CapsToggled) != 0; }
    bool IsNumToggled() const   { return (States & Key_NumToggled) != 0; }
    bool IsScrollToggled() const{ return (States & Key_ScrollToggled) != 0; }
    bool IsExtendedKey() const  { return (States & Key_ExtendedKey) != 0; }
    bool IsSpecialKeyPressed() const { return (States & (Key_ShiftPressed|Key_CtrlPressed|Key_AltPressed)) != 0; }

    void SetShiftPressed(bool v = true)  { (v) ? States |= Key_ShiftPressed : States &= ~Key_ShiftPressed; }
    void SetCtrlPressed(bool v = true)   { (v) ? States |= Key_CtrlPressed  : States &= ~Key_CtrlPressed; }
    void SetAltPressed(bool v = true)    { (v) ? States |= Key_AltPressed   : States &= ~Key_AltPressed; }
    void SetCapsToggled(bool v = true)   { (v) ? States |= Key_CapsToggled  : States &= ~Key_CapsToggled; }
    void SetNumToggled(bool v = true)    { (v) ? States |= Key_NumToggled   : States &= ~Key_NumToggled; }
    void SetScrollToggled(bool v = true) { (v) ? States |= Key_ScrollToggled: States &= ~Key_ScrollToggled; }
    void SetExtendedKey(bool v = true)   { (v) ? States |= Key_ExtendedKey  : States &= ~Key_ExtendedKey; }

    bool IsInitialized() const { return (States & Initialized_Mask) != 0; }
};

class Event
{
public:
    enum EventType
    {
        ET_UNKNOWN,
        ET_IME
    };

    // What kind of event this is.
    EventType       m_Type;

    // State of special keys
    KeyModifiers    m_KeyModifiers;

    // Size of class, used in debug build to verify that
    // appropriate classes are used for messages.
	unsigned int m_nEventClassSize;

    explicit Event( EventType eventType = ET_UNKNOWN )
    {
        m_Type = eventType;
        m_nEventClassSize = sizeof( Event );
    }

    Event( EventType eventType, KeyModifiers keysState )
    {
        m_Type = eventType;
        m_KeyModifiers = keysState;
        m_nEventClassSize = sizeof( Event );
    }
};

class IMEEvent : public Event
{
public:
    enum IMEEventType
    {
        IME_ET_DEFAULT,
        IME_ET_PREPROCESSKEYBOARD
    };

    IMEEvent( IMEEventType t = IME_ET_DEFAULT ) : Event( ET_IME ), m_IMEEventType( t )
    {
        m_nEventClassSize = sizeof( IMEEvent );
    }

	IMEEventType m_IMEEventType;
};

class IMEWin32Event : public IMEEvent
{
public:
    IMEWin32Event( IMEEventType t = IME_ET_DEFAULT ) : 
		IMEEvent( t ), 
		m_hWnd( 0 ), 
        m_nWin32MessageId( 0 ),
		m_wParam( 0 ), 
		m_lParam( 0 ),
		m_nOptions( 0 )
    {
        m_nEventClassSize = sizeof( IMEWin32Event );
    }

    IMEWin32Event( IMEEventType t, void *hWnd, uint32 win32msg, uintp wParam, uintp lParam, int nOptions = 0 ) : 
		IMEEvent( t ),
		m_hWnd( hWnd ),
        m_nWin32MessageId( win32msg ),
		m_wParam( wParam ),
		m_lParam( lParam ),
		m_nOptions( nOptions )
    {
        m_nEventClassSize = sizeof( IMEWin32Event );
    }

	void *m_hWnd;
	uint32 m_nWin32MessageId;  // Win32 message id (WM_<>)
    uintp m_wParam;
    uintp m_lParam;
    int m_nOptions;
};

class CUtlWString
{
public:
	CUtlWString()
	{
		InitEmpty();
	}

	CUtlWString( const wchar_t *pString )
	{
		InitEmpty();
		Set( pString );
	}

	CUtlWString( const CUtlWString &string )
	{
		InitEmpty();
		Set( string.Get() );
	}

	~CUtlWString()
	{
		Clear();
	}

	void Set( const wchar_t *pString )
	{
		Clear();

		m_nLengthInChars = pString ? V_wcslen( pString ) : 0;
		if ( m_nLengthInChars )
		{
			m_pString = new wchar_t[m_nLengthInChars + 1];
			V_memcpy( m_pString, pString, (m_nLengthInChars + 1) * sizeof( wchar_t ) );
		}
	}

	void Set( const char *pString )
	{
		Clear();

		if ( pString )
		{
			int nBytesNeeded = V_UTF8ToWString( pString, NULL, 0 );
			if ( nBytesNeeded )
			{
				nBytesNeeded += 2;
				m_pString = new wchar_t[nBytesNeeded];
				V_UTF8ToWString( pString, m_pString, nBytesNeeded );

				m_nLengthInChars = V_wcslen( m_pString );
			}
		}		
	}

	const wchar_t *Get() const
	{
		return ( m_pString ? m_pString : L"" );
	}

	// Release memory and reset
	void Clear()
	{
		delete [] m_pString;
		InitEmpty();
	}

	bool IsEmpty()
	{
		return ( m_nLengthInChars == 0 );
	}

	int Length() const
	{
		return m_nLengthInChars;
	}

	CUtlWString &operator=( const wchar_t *pSrc )
	{
		Set( pSrc );
		return *this;
	}

	CUtlWString &operator=( const char *pSrc )
	{
		Set( pSrc );
		return *this;
	}

private:
	void InitEmpty()
	{
		m_pString = NULL;
		m_nLengthInChars = 0;
	}

	wchar_t *m_pString;
	int m_nLengthInChars;
};

class CTempWStringToPrintableString
{
public:
	CTempWStringToPrintableString( const wchar_t *pwString )
	{
		m_pString = NULL;

		// each Unicode code point can expand to as many as four bytes in UTF-8; we
		// also need to leave room for the terminating NUL.
		uint32 cbMax = 4 * static_cast< uint32 >( V_wcslen( pwString ) ) + 1;
		char *pchTemp = new char[ cbMax ];
		if ( V_WStringToUTF8( pwString, pchTemp, cbMax ) )
		{
			uint32 cchAlloc = static_cast< uint32 >( V_strlen( pchTemp ) ) + 1;
			char *pchHeap = new char[ cchAlloc ];
			V_strncpy( pchHeap, pchTemp, cchAlloc );

			for ( uint32 i = 0; i < cchAlloc; i++ )
			{
				if ( (unsigned char)pchHeap[i] > 127 )
				{
					pchHeap[i] = '?';
				}
			}

			delete [] pchTemp;
			m_pString = pchHeap;
		}
	}
	
	~CTempWStringToPrintableString()
	{
		delete [] m_pString;
		m_pString = NULL;
	}

	const char *Get()
	{
		return m_pString ? m_pString : "";
	}

private:
	char *m_pString;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CIMEManager : public CTier2AppSystem< IIMEManager >
{
	typedef CTier2AppSystem< IIMEManager > BaseClass;

	// Methods of IAppSystem
public:
	virtual bool Connect( CreateInterfaceFn factory ) OVERRIDE;
	virtual const AppSystemInfo_t* GetDependencies() OVERRIDE;
	virtual	InitReturnVal_t Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;

	// Methods of IIMEManager
public:
	virtual bool Setup( PlatWindow_t hWindow ) OVERRIDE;
	virtual bool IsValid() OVERRIDE { return m_bValid; }

	virtual void SetIMEEnabled( bool bEnabled ) OVERRIDE;
	virtual bool IsIMEEnabled() OVERRIDE { return m_bIMEEnabled; }

	virtual bool HandleIMEEvent( PlatWindow_t hWindow, uint32 uMsg, uintp wParam, uintp lParam ) OVERRIDE;
	virtual bool HandlePreProcessKeyboardEvent( PlatWindow_t hWindow, uint32 uMsg, uintp wParam, uintp lParam ) OVERRIDE;
	virtual void HandleMouseDownEvent( IIMEUIView *pUIView, IIMEUIObject *pObjectUnderMouse ) OVERRIDE;
	virtual void HandleFocusChange( IIMEUIObject *pObject, bool bFocusSet ) OVERRIDE;

	virtual void SetActiveUIView( IIMEUIView *pUIView, bool bActive ) OVERRIDE;
	virtual IIMEUIView *GetActiveUIView() OVERRIDE;

	virtual LoggingChannelID_t GetLoggingChannel() OVERRIDE;

	CON_COMMAND_MEMBER_F( CIMEManager, "ime_info", IMEInfo_f, "Spew IME info.", FCVAR_DONTRECORD );

public:
	// Constructor, destructor
	CIMEManager();
	virtual ~CIMEManager();

private:
	GFxIMEManagerWin32 *m_pIMEManagerWin32;
	bool m_bValid;
	bool m_bIMEEnabled;
};

#endif

