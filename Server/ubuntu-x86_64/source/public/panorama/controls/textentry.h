//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTENTRY_H
#define PANORAMA_TEXTENTRY_H

#if defined(_WIN32) || defined(SOURCE2_PANORAMA)
#pragma once
#endif

#include "panel2d.h"
#include "panorama/text/iuitextlayout.h"
#include "panorama/textinput/textinput.h"
#include "panorama/textinput/textinput_settings.h"
#if !defined(SOURCE2_PANORAMA)
#include "../common/globals.h"
#include "../common/reliabletimer.h"
#endif
#include "panorama/uischeduleddel.h"
#include "imesystem/imeuiinterface.h"

namespace panorama
{

class CLabel;
class CTextEntryAutocomplete;

#if defined( SOURCE2_PANORAMA )
class CTextEntryIMEControls;
#endif // defined( SOURCE2_PANORAMA )

DECLARE_PANEL_EVENT1( TextEntrySubmit, const char * );		// always raised when user is done submitting text
DECLARE_PANEL_EVENT0( TextEntryChanged );					// call CTextEntry::RaiseChangeEvents() to enable
DECLARE_PANEL_EVENT0( TextEntryShowTextInputHandler );
DECLARE_PANEL_EVENT0( TextEntryHideTextInputHandler );
DECLARE_PANEL_EVENT0( TextEntryInsertFromClipboard );
DECLARE_PANEL_EVENT0( TextEntryCopyToClipboard );
DECLARE_PANEL_EVENT0( TextEntryCutToClipboard );
DECLARE_PANEL_EVENT0( TextEntryUpdate );
DECLARE_PANEL_EVENT1( TextEntrySetText, const char * );

//-----------------------------------------------------------------------------
// Purpose: text entry
//-----------------------------------------------------------------------------
class CTextEntry : public CPanel2D, public ITextInputControl
#if defined( SOURCE2_PANORAMA )
	, public IIMEUITextField
#endif // defined( SOURCE2_PANORAMA )
{
	DECLARE_PANEL2D( CTextEntry, CPanel2D );

public:
	CTextEntry( CPanel2D *parent, const char * pchPanelID );
	CTextEntry( CPanel2D *parent, const char * pchPanelID, const CTextInputHandlerSettings& settings );
	virtual ~CTextEntry();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	void SetUndoHistoryEnabled( bool bEnabled );
	void ClearUndoHistory();

	virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
	virtual bool OnKeyUp( const KeyData_t & code ) OVERRIDE { return BaseClass::OnKeyUp( code ); }
	virtual void Paint() OVERRIDE;
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnInitializedFromLayout() OVERRIDE;
	virtual void OnStylesChanged() OVERRIDE;

	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDoubleClick( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonTripleClick( const MouseData_t &code ) OVERRIDE;

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	virtual bool BRequiresContentClipLayer() OVERRIDE { return m_bMayDrawOutsideBounds; }

	void SetMode( ETextInputMode_t mode );
	ETextInputMode_t GetMode() { return m_modeInput; }

	void SetText( const char *pchValue );
	virtual const char *PchGetText() const OVERRIDE;
	virtual const uchar32 *Pch32GetText() const OVERRIDE;
	void SetAllowRawPlaceholderText( bool bAllow );
	void SetPlaceholderText( const char *pchValue );
	const char *PchGetPlaceholderText() const;
	void SetMaxChars( uint unMaxChars );
	virtual uint GetCharCount() const OVERRIDE { return m_vecUniCharData.Count() - 1; }	// m_vecUniCharData is terminated, so remove extra char count
	uint GetMaxCharCount() const { return m_unMaxChars; }
	
	void SetMultiline( bool bMultiline );

	// Insert clipboard contents into text entry
	void InsertFromClipboard();

	// Cut selected text to clipboard
	void CutToClipboard();

	// Copy selected text to clipboard
	void CopyToClipboard();

	bool OnInsertFromClipboard( const CPanelPtr< IUIPanel > &pPanel );
	bool OnCutToClipboard( const CPanelPtr< IUIPanel > &pPanel );
	bool OnCopyToClipboard( const CPanelPtr< IUIPanel > &pPanel );

	// Enable/disable context menu for cut/copy/paste
	void SetContextMenuEnabled( bool bEnabled );

	void SetAlwaysRenderCaret( bool bAlwaysRenderCaret );

	// Delete currently selected text 
	void DeleteSelection( bool bDontPushUndoHistory );

	// Clear selection region, leaving nothing selected
	void ClearSelection();

	// select all the text in the control
	void SelectAll();

	// Lock the selection in place; it can only be unlocked or deleted
	void LockSelection( bool bLockSelection ) { m_bSelectionLocked = bLockSelection; }

	bool BIsValidCharacter( const uchar32 ch32 );

	void RaiseChangeEvents( bool bEnable ) { m_bRaiseChangeEvents = bEnable; }

	virtual EMouseCursors GetMouseCursor() OVERRIDE;

	// ITextInputControl helpers

	virtual bool BSupportsImmediateTextReturn() OVERRIDE { return true; }

	virtual int32 GetCursorOffset() const OVERRIDE { return m_nCursorOffset; }
	virtual void SetCursorOffset( int32 nCursoroffset );

	virtual void InsertCharacterAtCursor( const uchar32 &unichar ) OVERRIDE;
	virtual void InsertCharactersAtCursor( const uchar32 *pch32, size_t cch32 ) OVERRIDE;

	virtual CPanel2D *GetAssociatedPanel() OVERRIDE{ return this; }
	virtual void OnTextInputHandlerOpened( CTextInputHandler *pHandler ) OVERRIDE;

	virtual void RequestControlString() OVERRIDE { Assert( false ); } // no op, we already have the string
	

	panorama::CTextInputHandler *GetTextInputHandler() { return m_pTextInputHandler.Get(); }

	virtual bool BRequiresFocus() OVERRIDE { return true; }

	// Autocomplete
	void ClearAutocomplete( void );
	void AddAutocomplete( const char *pszOption );
	void AddAutocompletePanel( CPanel2D *pPanelOption, const char* pszAutocompleteText );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif
	int32 GetSelectionStart() { return m_nSelectionStartIndex; }
	int32 GetSelectionEnd() { return m_nSelectionEndIndex; }

	// Interface for IIMEUITextField
#if defined( SOURCE2_PANORAMA )
	virtual void IME_SetLoggingChannel( LoggingChannelID_t loggingChannel ) OVERRIDE;
	virtual bool IME_IsEnabled() OVERRIDE;
	virtual IMEUIObjectType IME_GetObjectType() OVERRIDE;
	virtual uchar32 *IME_GetCompositionString() OVERRIDE;
	virtual void IME_CreateCompositionString() OVERRIDE;
	virtual void IME_ClearCompositionString() OVERRIDE;
	virtual void IME_CommitCompositionString( const uchar32 *pString ) OVERRIDE;
	virtual void IME_SetCompositionStringText( const uchar32 *pString ) OVERRIDE;
	virtual void IME_SetCompositionStringPosition( uint32 nPos ) OVERRIDE;
	virtual void IME_SetCursorInCompositionString( uint32 nPos ) OVERRIDE;
	virtual uint32 IME_GetCaretIndex() OVERRIDE;
	virtual uint32 IME_GetBeginIndex() OVERRIDE;
	virtual uint32 IME_GetEndIndex() OVERRIDE;
	virtual void IME_ReplaceCharacters( const uchar32 *pString, uint32 nStart, uint32 nEnd ) OVERRIDE;
	virtual void IME_SetSelection( uint32 nStart, uint32 nEnd ) OVERRIDE;
	virtual void IME_SetWideCursor( bool bWide ) OVERRIDE;
	virtual void IME_HighlightCompositionStringText( uint32 nPos, uint32 nLen, IMETextHighlightStyle highlightStyle ) OVERRIDE;
	virtual void IME_DeleteSelection() OVERRIDE;
	virtual	void IME_RemoveInputWindow() OVERRIDE;
	virtual	void IME_DisplayInputWindow( const uchar32 *pReadingString, const IMERectF *pPosition ) OVERRIDE;
	virtual	void IME_RepositionInputWindow( const IMERectF *pPosition ) OVERRIDE;
	virtual	void IME_CreateList( int nPageSize, int nListStartsAt1 ) OVERRIDE;
	virtual	void IME_RemoveList() OVERRIDE;
	virtual	void IME_ClearList() OVERRIDE;
	virtual	void IME_ShowList( bool bShow ) OVERRIDE;
	virtual	void IME_RepositionCandidateList( const IMERectF *pPosition ) OVERRIDE;
	virtual	void IME_SelectItemInList( int32 nItemToSelect ) OVERRIDE;
	virtual	void IME_AddToList( const uchar32 *pCandidateString ) OVERRIDE;
	virtual	void IME_SetBackspaceFilter( bool bFilterNextBackspace ) OVERRIDE;
#endif // defined( SOURCE2_PANORAMA )

protected:
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE;
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;

private:
	void CommonConstructor( CPanel2D *parent, const char * pchPanelID, const CTextInputHandlerSettings& settings );

	bool OnTextEntryShowTextInputHandler( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool OnTextEntryHideTextInputHandler( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool EventActivated ( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool OnTextEntryScrollToCursor( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool EventInputFocusTopLevelChanged( CPanelPtr< IUIPanel > ptrPanel );
	bool HandleTextInputFinished( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, bool bFinished, char const *pchText );
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventPropertyTransitionEnd( const CPanelPtr< IUIPanel > &ptrPanel, CStyleSymbol prop );
	bool EventTextEntryUpdate( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventSetText( const CPanelPtr< IUIPanel > &ptrPanel, const char *pchText );

	IUITextLayout *CreateTextLayout( float flWidth, float flHeight );
	void RemoveCharacter( int32 offset );
	void RaiseTextChangedEvent();

	void UpdateSelectionToInclude( int32 unPreviousCursor, int32 unNewCursorPos );

	void Undo();
	void Redo();
	void PushUndoStack();
	void PushRedoStack();

	void UpdateCapsLockWarning();
	void OnScheduledCapsLockCheck();

	void MoveCaretToEnd( bool bIsShiftHeld );

	void ShowVRKeyboard();

	bool m_bDeferShowKeyboard;
	double m_fDeferShowKeyboardTime;

	const uchar32 *Pch32GetTextDisplay(); // honors password-entry mode

	bool m_bUndoHistoryEnabled;

	bool m_bContentSizeDirty;				// content size is dirty - text has changed

	float m_flMaxWidthLastContentSize;
	float m_flMaxHeightLastContentSize;

	bool m_bCaretPositionDirty;
	bool m_bAlwaysRenderCaret;

	bool m_bMayDrawOutsideBounds;

	bool m_bShowTextInputHandlerOnLeftMouseUp;

	bool m_bSelectionLocked;
	bool m_bMultiline;					// used to determine whether to swallow multiline-only characters (\n, etc.)
	bool m_bContextMenuEnabled;			// whether to show context menu for cut/copy/paste

	Vector2D m_LastMousePos;
	CUtlVector<uchar32> m_vecUniCharData;
	CUtlVector<uchar32> m_vecUniCharDataDisplayFormatted;
	mutable CUtlString m_UTF8String;
	mutable bool m_bUTF8StringInvalid;
	uint32 m_unMaxChars;

	int32 m_nCursorOffset;
	Vector2D m_CaretCoords;
	float m_flCaretHeight;

	bool m_bLeftMouseIsDown;
	bool m_bSelectionRectDirty;
	int32 m_nSelectionStartIndex;
	int32 m_nSelectionEndIndex;

	bool m_bScrollableSizeDirty;
	float m_flLastFinalWidthToScrollable;
	float m_flLastFinalHeightToScrollable;

	// Translation of text in single line entries to scroll to show where the cursor is
	float m_flTextXTranslate;
   
	CPanelPtr< CTextInputHandler > m_pTextInputHandler; 

	CUtlVector<IUITextLayout::HitTestRegionRect_t> m_vecSelectionRects;
	panorama::CTextInputHandlerSettings m_settingsTextInput;

	CUtlVector< uchar32 * > m_vecUndoStack;
	CUtlVector< uchar32 * > m_vecRedoStack;

	double m_flFocusTime;

	// only raise text changed events when requested, as we have to convert every character to UTF8 from unicode
	bool m_bRaiseChangeEvents;

	ETextInputMode_t m_modeInput;

	bool m_bDisplayInput;
	bool m_bWarnOnCapsLock;
	panorama::CUIScheduledDel m_scheduledCapsLockCheck;

	bool m_bAllowRawPlaceholderText;
	CLabel *m_pPlaceholderText;
	
	CPanelPtr< CTextEntryAutocomplete > m_pAutocompleteMenu;

#if defined( SOURCE2_PANORAMA )
	// IME State
	bool m_bIMEWideCursor;
	CUtlVector< uchar32 > m_IMECompositionString;
	int32 m_nIMECompositionCursor;
	int32 m_nIMEStartingCursorInsertionOffset;
	int32 m_nIMEEndingCursorInsertionOffset;
	bool m_bIMERejectBackspace;

	CPanelPtr< CTextEntryIMEControls > m_pIMEControls;
	LoggingChannelID_t m_IMELoggingChannel;
#endif // defined( SOURCE2_PANORAMA )
};

class CTextEntryAutocomplete : public CPanel2D
{
	DECLARE_PANEL2D( CTextEntryAutocomplete, CPanel2D );

public:
	CTextEntryAutocomplete( CTextEntry *pParent, const char *pchPanelID );
	virtual ~CTextEntryAutocomplete();

	void DeleteSelf( bool bSetFocusToTarget = true );

	void AddOption( CPanel2D *pPanel );

private:
	// forward keys, arrows back to my parent
	virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
	virtual bool OnKeyUp( const KeyData_t & code ) OVERRIDE;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;

	void PositionNearParent();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;

	// events
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &pPanel );

	void SuggestionSelected( CPanel2D *pPanel );

	CPanelPtr< CTextEntry > m_pTextEntryParent;
};

#if defined( SOURCE2_PANORAMA )
class CTextEntryIMEControls : public CPanel2D
{
	DECLARE_PANEL2D( CTextEntryIMEControls, CPanel2D );

public:
	CTextEntryIMEControls( CTextEntry *pParent, const char *pchPanelID );
	virtual ~CTextEntryIMEControls();

	void ClearCandidateList();
	void CreateCandidateList( int nPageSize, int nListStartsAt1 );
	void AddCandidate( const uchar32 *pszCandidateString );
	void SetSelectedCandidate( int nItemToSelect );
	void ShowCandidateList( bool bShow );

	void SetReadingString( const uchar32 *pReadingString );

private:
	void PositionNearParent();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	CPanelPtr< CTextEntry > m_pTextEntryParent;

	CPanelPtr< CLabel > m_pReadingStringLabel;
	CPanelPtr< CPanel2D > m_pCandidateList;

	int m_nCandidateListPageSize;
	int m_nCandidateListSelectedIndex;

	bool m_bShowCandidateList;
};
#endif // defined( SOURCE2_PANORAMA )

} // namespace panorama

#endif // PANORAMA_TEXTENTRY_H
