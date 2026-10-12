/**************************************************************************

Filename    :   GFx_IMEWin32Impl.h 
Content     :   Contains declarations for GFxIMEWin32Impl, GFxIMEVista, GFxIMEXP. 
Created     :   OCt 4, 2007
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef _GFX_IMEWIN32IMPL_H_
#define _GFX_IMEWIN32IMPL_H_

#include "gfx_candidatelistbox.h"
#include "gfx_imemanager.h"
#include "gfx_imeidmap.h"

struct styleInfo
{
	styleInfo()
	{
		m_nBegin = 0;
		m_nEnd = 0;
		m_nHighLightStyle = 0;
	}

    uint32 m_nBegin;
    uint32 m_nEnd;
    uint32 m_nHighLightStyle;
};

class GFxIMEWin32Impl
{
	friend class CTSF;

public:
    GFxIMEWin32Impl( IMEManagerBase *pIMEManagerBase, HWND hWnd );  
    virtual ~GFxIMEWin32Impl();

	// Checks if cursor is in a textfield. For any other element, IME messages/TSF notifications are discarded
	virtual bool IsTextFieldFocused( IIMEUIView *pUIView = NULL );

    virtual void HandleStatusWindowNotifications(const char* pcommand, const char* parg);

    // Handles WM_IMECOMPOSITION
    virtual void OnIMEComposition( uintp wParam, uintp lParam, int nOptions );

    // Handles WM_IME_STARTCOMPOSITION
	virtual IMEEventResult OnIMEStartComposition( uintp wParam, uintp lParam, bool bDownFlag );

    // Handles WM_IMEENDCOMPOSITION
    virtual void OnIMEEndComposition( uintp wParam, uintp lParam, bool bDownFlag );

    // Handles WM_IME_NOTIFY for candidate list related notifications
    virtual LRESULT OnIMENotify( uint32 message, uintp wParam, uintp lParam, bool bDownFlag );

    // Handles some WM_NOTIFY messages that get sent when backspace, space, esc keys etc are pushed.
    virtual void CustomProcessing( uint32 message, uintp wParam, uintp lParam );
   
    // Finalizes IME composition text, closes any pop ups (candidate list, reading window)
    virtual void Finalize( bool bCancel = false );

    virtual void Cleanup();

    virtual void SetIMETag( GFxIMETag tag );

    void SetIMEVersionId( int nIMEVersionId );

    virtual void Shutdown();

    // Selects a particular row of the Candidate List (specified by index). Doesn't finalize
    virtual void SelectAndClose(int index);

    virtual void FsCallBack( const char* pcommand, const char* parg);
    
    // Records the last keystroke for reading window display
    virtual void PreProcessHandler(const IMEWin32Event& winEvt);

    // Takes care of interacting with AS to display reading (input) window
    virtual void DisplayReadingWindow(const uchar32* pReadingStr);
    
    // Displays composition string, keeps track of how many characters are on the screen, cursor position etc
    virtual void DisplayCompositionString(uint32& numCharDisplay);

    // Checks if we are in a textfield or not. If not, translate message is not called. 
	// This is to make TAB work correctly when IME is active and focus is on a non textfield object.
    virtual bool CheckIfInTextField();
   
    virtual void SetOpenStatus(bool status);

    // This is used to turn IME on/off when the user clicks to an editable field (ime = on) or
    // a non-editable field (ime = off). In addition, user can also SetEnabled AS function to
    // turn IME on/off. 
    virtual void OnEnableIME(bool enable );

    virtual const char *GetInputLanguage();

    virtual bool SetEnabled(bool enabled);

    virtual bool GetEnabled();

    virtual bool SetCompositionString(const wchar_t *pCompString);

    virtual bool SetConversionMode(const uint32 conversionMode);

    // Retrieves conversion mode. 
    virtual const char *GetConversionMode();

	void Invoke_RemoveInputWindow();
	void Invoke_DisplayInputWindow( const uchar32 *pReadingString, const IMERectF *pPosition );
	void Invoke_RepositionInputWindow( const IMERectF *pPosition );
	void Invoke_CreateList( int nPageSize, int nListStartsAt1 );
	void Invoke_RemoveList();
	void Invoke_ClearList();
	void Invoke_ShowList( bool bShow );
	void Invoke_RepositionCandidateList( const IMERectF *pPosition );
	void Invoke_SelectItemInList( int32 nItemToSelect );
	void Invoke_AddToList( const wchar_t *pCandidateString );

protected:
    HWND                m_hWnd;

    // For Candidate List listbox
    CandidateListBox*   m_pCandidateListBox;
  
    enum                { ReadingTextBufferSize = 256 };
	CUtlVector< uchar32 > m_ReadingTextBuffer; // Buffer for containing reading window characters  

	void SetReadingText( const uchar32 *pStr )
	{
		int nLen = V_strlen32( pStr ) + 1;
		m_ReadingTextBuffer.SetCount( nLen );
		memcpy( m_ReadingTextBuffer.Base(), pStr, ( nLen * sizeof(*pStr) ) );
	}

    HKL                 m_CurrLocale; 

    // Indicates the number of characters displayed last time 
    uint32				m_nNumCharDisplay;

    // This tells us the position of the first character in the currently highlighted clause
    // Its used to adjust the position of the candidate list window.
    int                 m_nClausePosition; 

    // Used to record if we should write over the current character or write a new one 
    // in Korean IME
    int                 m_bReplaceChar; 

    // Records the number of candidates on the candidate list on the previous 
    // display. 
    int                 m_nNumCandidatesOnListPrev; 

    // Tells us the index from which the current page starts in the list of candidate strings
    int                 m_nCurrentPageStart;
    
    // Stores the current conversion mode
    CUtlString			m_ConversionMode;

    // Global IME State
    bool                m_bGlobalIMEState;

    union 
    {
        HKL     hkl;
        size_t  val;
    } Un;

    GFxIMETag           m_IMETag;

    int                 m_nIMEVersionId;

	// Used to keep a track of IME messages- used in PreProcessHandler.
	bool                m_bTrack;
	bool                m_bIMEMessageRecd;

	IMEManagerBase		*m_pIMEManagerBase;
};

//
// This class implements IME on Windows Vista. It uses the TSF (Text Services Framework) architecture
// for handling CandidateList and Reading Window logic. 
// We decided to use TSF for implementing IME on Windows Vista for the following reasons:
//
// 1) TSF always provides us with the correct Reading window text information. 
//    Unlike IMM where reading window text information is not available for some IME's
//    for example- New Phonetic, New Chang Jie, Quan Pin. The lack of this information 
//    complicates IME implementation for XP where we have to provide keyboard mapping 
//    tables for different IMEs. 
//
// 2) IMM doesn't provide information on when to close the Candidate List on Windows Vista.
// 
// In addition to the above mentioned reasons, there are a number of other inconsistencies between
// IMM documentation as provided on MSDN and actual IMM behaviour. 
//
// We use TSF only for handling Candidate List and Reading Window logic. The composition string,
// cursor positioning and attribute logic is still handled using IMM.
//
class GFxIMEVista: public GFxIMEWin32Impl
{
public:
    GFxIMEVista( IMEManagerBase* pbase, HWND hwnd );
    ~GFxIMEVista();

	virtual void    DisplayReadingWindow(const uchar32* pReadingStr);
	virtual void    RepositionCandidateList(uint32 offset);

	virtual void	OnEnableIME(bool enable);

    virtual void    OnIMEComposition( uintp wParam, uintp lParam, int nOptions );    
    virtual IMEEventResult  OnIMEStartComposition(uintp wParam, uintp lParam, bool downFlag);   
    virtual void    OnIMEEndComposition(uintp wParam, uintp lParam, bool downFlag);   
    virtual void    OnChangeCandidate(int candListIndex);
    virtual void    OnOpenCandidate(int candListIndex);
    virtual LRESULT OnIMENotify(uint32 message, uintp wParam, uintp lParam, bool downFlag);

    virtual void    Finalize( bool bCancel = false );
    virtual void    SelectAndClose(int index); // Selects the text in row # and closes Candidate List 
    virtual void    FsCallBack( IIMEUIView *pUIView, const char* pcommand, const char* parg);
    virtual void    PreProcessHandler(const IMEWin32Event& winEvt);
    virtual void    DisplayCompositionString(uint32& numCharDisplay);

	CUtlWString     m_ReadingStringBuffer;

	bool			m_bStartCompositionReceived;
	bool			m_bDrawCandidateList;
	
	// Maintains reference count for the UI elements ( incremented in BeginUIElement, decremented in EndUIElement)
	int				m_nCandidateWindowRefCount;

	// Used to record if there is a pending DisableIME request that couldn't be satisfied since there are open UI elements. Therefore, OnEnableIME
	// should be called again when the UI elements have been closed. 
	bool			m_bNeedToDisableIME;

private:
    void            CloseIME(); 

    CTSF			*m_pTSF; 

    int             m_nPrevX; // Records the previous location of the candidate list
    int             m_nPrevY; 
    int             m_nNumCandidates;
 
     // This variable tells us if the candidate list starts from 1 or 0.
    int             m_nCandListStartFrom1;

    // Union to cast HKL to size_t
    union HklUn
	{
        HKL     hkl;
        size_t  langId;
    } Un;
};

//
//	This class contains IME implementation for Windows XP. We use the input method editor (IMM) for 
//	implementing IME support. When a user interacts with an IME to input complex characters, the 
//	IMM sends messages to the application to notify it of important events, such as starting a 
//	composition or showing the candidate window. This class provides functions to handle these
//	messages as well as fscallbacks from the candidate list.
//
class GFxIMEXP: public GFxIMEWin32Impl
{   
public:
    GFxIMEXP( IMEManagerBase *pBase, HWND hWnd );
    ~GFxIMEXP();

private:
    virtual void		OnIMEComposition( uintp wParam, uintp lParam, int nOptions );
    virtual IMEEventResult OnIMEStartComposition(uintp wParam, uintp lParam, bool downFlag);
    virtual void		OnIMEEndComposition(uintp wParam, uintp lParam, bool downFlag);
    virtual void		OnChangeCandidate(int candListIndex);
    virtual void		OnOpenCandidate(int candListIndex);

    virtual void        Finalize( bool bCancel = false );
    virtual void        Cleanup();

    virtual void        SelectAndClose(int index); // Selects the text in row # and closes Candidate List 

    virtual void        FsCallBack( IIMEUIView *pUIView, const char* pcommand, const char* parg);

    virtual void        PreProcessHandler(const IMEWin32Event& winEvt);

	void                DisplayReadingWindow(const uchar32* lpReadStr) { NOTE_UNUSED(lpReadStr); };

    void                DisplayReadingWindow(uint32 keyCode, int numChar, char* lpastr, const uchar32* lpReadStr);

    virtual void        DisplayCompositionString(uint32& numCharDisplay);

    void                SetLastCharacter(char lastChar){ m_LastCharacter = lastChar; }

    void                CustomProcessing( uint32 message, uintp wParam, uintp lParam);

    virtual LRESULT     OnIMENotify(uint32 message, uintp wParam, uintp lParam, bool downFlag);

    void                CloseIME(); 

	void				RepositionCandidateList(uint32 offset);

    // Buffer for Chinese Traditional (New Phonetic)
	CUtlVector< uchar32 > m_ReadingTextChTradNewPhonetic;

    // Indicates which position in the reading window to Place current character
    // Used by Chinese Trad New Phonetic, where Reading Window chars (strokes) appear at
    // predetermined positions in the Reading Window. 
    int                 m_nReadingWindowCharacterPos; 
    int                 m_nNumCandidates;
    uchar32             m_LastCharacter;
    int                 m_nNumRowsMax;
    
    enum                { NumTradNewPhoneticKeyCodes = 102 };
    int                 m_TradNewPhoneticKeyCodesB[NumTradNewPhoneticKeyCodes][3];

    // This variable tells us if the candidate list starts from 1 or 0.
    int                 m_nCandListStartFrom1; 

    // Used with Chinese Simplified (MS Pinyin) reading window display. 
    int                 m_nCurrChar;   

    // Union to cast HKL to size_t
    union HklUn
	{
        HKL     hkl;
        size_t  langId;
    } Un;

    int                 m_nCandidateListIndex;
	bool				m_bDrawCandidateList;
}; 

#endif

