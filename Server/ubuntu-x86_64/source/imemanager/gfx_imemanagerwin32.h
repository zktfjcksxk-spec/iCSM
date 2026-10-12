/**************************************************************************

Filename    :   GFx_IMEManagerWin32.h
Content     :   Implementation of Input Method Support on Win32 platforms.
Created     :   OCt 4, 2007
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef _GFX_IMEMANAGERWIN32_H_
#define _GFX_IMEMANAGERWIN32_H_

#include "gfx_imemanager.h"
#include "gfx_imeidmap.h"

class GFxIMEWin32Impl;

// Public interface for Input Method Editor.

class GFxIMEManagerWin32 : public IMEManagerBase
{
public:
    GFxIMEManagerWin32( HWND hWnd );
    ~GFxIMEManagerWin32();
   
    virtual bool Init();

    virtual void SetActiveUIView( IIMEUIView *pUIView );

    // handles IME events
    virtual IMEEventResult HandleIMEEvent( IIMEUIView *pUIView, const IMEWin32Event &imeWin32Event );

    // Finalizes IME composition text, closes any pop ups (candidate list, reading window)
    virtual void OnFinalize( bool bCancel );

    virtual void OnShutdown();

    virtual void OnEnableIME(bool enable);

    // Handles WM_IME_NOTIFY for candidate list related notifications
    LRESULT OnIMENotify( uint32 message, uintp wParam, uintp lParam, bool bDownFlag );

    // Sets Input Language
    void SetInputLanguage(const char* inputLanguage);

    // Sets Conversion Mode
    bool SetConversionMode(const uint32 convMode);

    // Retrieves Conversion Mode
    const char *GetConversionMode();

    // Enables/Disables IME. 
    virtual bool SetEnabled(bool enable); 

    // Retrieves IME state. 
    virtual bool GetEnabled(); 

    const char *GetInputLanguage();

    void SetCurrentInputLanguage(GFxIMETag IMETag);

    virtual bool SetCompositionString(const wchar_t *pCompString);

    // Handles some WM_NOTIFY messages that get sent when backspace, space, esc keys etc
    // are pushed.
    void CustomProcessing( uint32 message, uintp wParam, uintp lParam );

    // Selects a particular row of the Candidate List (specified by index). Doesn't finalize
    void SelectAndClose(int index); // Selects the text in row # and closes Candidate List

	// Records the last keystroke for reading window display
	void PreProcessHandler(const IMEWin32Event& winEvt);

	// Takes care of interacting with AS to display reading (input) window
	void DisplayReadingWindow( const uchar32 *pReadingStr );

    // Displays composition string, keeps track of how many characters are on the screen, cursor
    // position etc
    void DisplayCompositionString(uint32& numCharDisplay);

    // Checks if we are in a textfield or not. If not, translate message is not called. This
    //   is to make TAB work correctly when IME is active and focus is on a non textfield object.
    bool CheckIfInTextField();

	CUtlString GetSystemLanguageInfo();

	enum OSVersion
	{
		OSVER_UNKNOWN = 0,
		OSVER_WIN2K,
		OSVER_WINXP,
		OSVER_WINVISTA,
		OSVER_WIN7,
		OSVER_WIN8,
		OSVER_WIN81,
		OSVER_WIN10,
	};
    OSVersion DetectWindowsVersion();

	void SetIMETag( GFxIMETag imeTag ); 

    void SetIMEVersionId( int versionId ); 

public:
    HWND					m_hWnd;
   
    // This class implements Input Method Editor functionality.
    class GFxIMEWin32Impl	*m_pIMEImpl;
    class CIMENamesManager	*m_pIMENamesMgr;

    GFxIMETag               m_IMETag;

	CUtlWString				m_CurrentLanguage;

    bool                    m_bIMEOpenStatus;
    bool                    m_bNamesManagerInitStatus;

    friend void             CIMENamesManager::SetLastIMEName( const wchar_t *pIMEName );

    OSVersion               m_OSVersion;

    int                     m_nIMEVersionId;
};

#endif //INC_IMEWIN32_H
