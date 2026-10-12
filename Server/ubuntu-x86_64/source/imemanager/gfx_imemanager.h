/**************************************************************************

Filename    :   GFx_IMEManager.h
Content     :   
Created     :   Mar 27, 2008
Authors     :   A. Bolgar

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/
// IME manager interface class. This class may be used as a base class for various
// IME implementations. It also provides utility functions to control composition, 
// candidate list and so on.

#ifndef _GFX_IMEMANAGER_H_
#define _GFX_IMEMANAGER_H_

#define GFX_CANDIDATELIST_LEVEL 9999
#define GFX_CANDIDATELIST_FONTNAME "$IMECandidateListFont"

//class IIMEUIObject;
class IMEEvent;
class IIMEUITextField;

class IMEManagerBase
{
public:	
	IMEManagerBase();
	~IMEManagerBase();

	virtual bool Init();

//DS2	void ASRootMovieCreated(Ptr<Sprite> spr) { if (pASIMEManager) pASIMEManager->ASRootMovieCreated(spr);}

	// Handles IME events, calling callbacks and switching states.
	virtual IMEEventResult HandleIMEEvent( IIMEUIView* pUIView, const IMEEvent &imeEvent );

    void HandleMouseDownEvent( IIMEUIView *pUIView, IIMEUIObject *pItemUnderMouse );
	
	void HandleFocusChange( IIMEUIObject *pObject, bool bFocusSet );

    // creates the composition string, if not created yet
    void StartComposition();

    // finalizes the composition string by inserting the string into the
    // actual text. If pstr is not NULL then the content of pstr is being used;
    // otherwise, the current content of composition string will be used.
    void FinalizeComposition( const wchar_t *pString );

    // clears the composition string. FinalizeComposition with pstr != NULL
    // still may be used after ClearComposition is invoked.
    void ClearComposition();

    // changes the text in composition string
    void SetCompositionText( const wchar_t *pString );

    // relocates the composition string to the current caret position
    void SetCompositionPosition();

    // sets cursor inside the composition string. "pos" is specified relative to 
    // composition string.
    void SetCursorInComposition( uint32 nPos );

    // turns on/off wide cursor.
    void SetWideCursor( bool bWide = true );

	// Sets conversion mode. Base class version does nothing.
	virtual bool SetConversionMode(const uint32 convMode);

	// retrieves conversion mode. Base class version does nothing.
	virtual const char *GetConversionMode();

	// Enables/Disables IME. 
	virtual bool SetEnabled( bool bEnable );

	// Retrieves IME state. 
	virtual bool GetEnabled();

	// Retrieves current input language
	virtual const char *GetInputLanguage();

	// Support for OnIMEComposition event
	virtual void BroadcastIMEConversion( const wchar_t* pString );
  
	// Support for OnSwitchLanguage event
	virtual void BroadcastSwitchLanguage( const wchar_t *pString );

	// Support for OnSetSupportedLanguages event
	virtual void BroadcastSetSupportedLanguages( const wchar_t *pString );

	// Support for OnSetSupportedIMEs event
	virtual void BroadcastSetSupportedIMEs( const wchar_t *pString );

	// Support for OnSetCurrentInputLanguage event
	virtual void BroadcastSetCurrentInputLanguage( const wchar_t *pString );

	// Support for OnSetIMEName event
	virtual void BroadcastSetIMEName( const wchar_t *pString );

	// Support for OnSetConversionStatus event
	virtual void BroadcastSetConversionStatus( const wchar_t *pString );

	// Support for OnBroadcastRemoveStatusWindow event
	virtual void BroadcastRemoveStatusWindow();

	// Support for OnBroadcastDisplayStatusWindow event
	virtual void BroadcastDisplayStatusWindow();
	// SetCompositionString
	virtual bool SetCompositionString( const wchar_t *pCompString );

	// GetCompositionString
	virtual const uchar32* GetCompositionString(); 

	virtual void OnShutdown();

	// returns view rectangle of currently focused text field
	// and cursor rectangle, both in stage (root) coordinate space.
	// cursorOffset may be negative, specifies the offset from the 
	// actual cursor pos.
	void GetMetrics( IMERectF* pviewRect, IMERectF* pcursorRect, int cursorOffset = 0 );

	// highlights the clause in composition string.
	// Parameter "clause" should be true, if this method is called to highlight
	// the clause (for example, for Japanese IME). In this case, in addition to the
	// requested highlighting whole composition string will be underline by single
	// underline.
	void HighlightText( uint32 pos, uint32 len, IMETextHighlightStyle style, bool clause);

	// Finalize the composition and release the text field.
	void DoFinalize( bool bCancel = false );

	// invoked when need to finalize the composition.
	virtual void OnFinalize( bool bCancel );
	
	// enables/disables IME
	void EnableIME( bool bEnable );
	bool IsIMEEnabled() { return m_bIMEEnabled; }

	// handles enabling/disabling IME, invoked from EnableIME method
	virtual void OnEnableIME(bool enable);

	// sets currently focused ui view to IME manager.
	virtual void SetActiveUIView( IIMEUIView *pUIView );
	virtual bool IsUIViewActive( IIMEUIView *pUIView ) const;
	virtual IIMEUIView *GetActiveUIView() const;

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

	IIMEUIView			*m_pUIView;
	IIMEUITextField		*m_pActiveComposingTextField;
	uint32              m_nCursorPosition;
	bool                m_bIMEEnabled;
};

#endif
