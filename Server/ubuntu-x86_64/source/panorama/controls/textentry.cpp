//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/textentry.h"
#include "panorama/uievent.h"
#include "panorama/controls/contextmenu.h"
#include "panorama/controls/label.h"
#include "panorama/controls/dropdown.h"
#include "panorama/uijsregistration.h"
#include "panorama/localization/ilocalize.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/renderer/styleproperties.h"

#if !defined( SOURCE2_PANORAMA )
#include <vrapi.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

extern EStringTransformStyle ETextTransformToEStringTransformStyle( ETextTransform eTextTransform );

REGISTER_PANEL2D( CTextEntryAutocomplete, TextEntryAutocomplete );
#if defined( SOURCE2_PANORAMA )
REGISTER_PANEL2D( CTextEntryIMEControls, TextEntryIMEControls );
#endif // defined( SOURCE2_PANORAMA )

REGISTER_PANEL2D_FACTORY( CTextEntry, TextEntry );
DEFINE_PANORAMA_EVENT( TextEntrySubmit );
DEFINE_PANORAMA_EVENT( TextEntryChanged );
DEFINE_PANORAMA_EVENT( TextEntryShowTextInputHandler );
DEFINE_PANORAMA_EVENT( TextEntryHideTextInputHandler );
DEFINE_PANORAMA_EVENT( TextEntryUpdate );

DECLARE_PANEL_EVENT0( TextEntryScrollToCursor );
DEFINE_PANORAMA_EVENT( TextEntryScrollToCursor );
DEFINE_PANORAMA_EVENT( TextEntryInsertFromClipboard );
DEFINE_PANORAMA_EVENT( TextEntryCopyToClipboard );
DEFINE_PANORAMA_EVENT( TextEntryCutToClipboard );
DEFINE_PANORAMA_EVENT( TextEntrySetText );

const char * k_pchOnInputSubmit( "oninputsubmit" );
const char * k_pchOnTextEntrySubmit( "ontextentrysubmit" );
const char * k_pchOnTextEntryChange( "ontextentrychange" );
const int k_unUndoStackMax = 64;
const uchar32 k_ch32Mask = 0x25CF; // U+25CF BLACK CIRCLE


#if defined( SOURCE2_PANORAMA )
#define COLORMIN 160
#define COLORMAX 255
#define GET_BIT( val, bitnum ) ( ( val >> bitnum ) & 0x1 )

Color &IME_GetFunctionColor( const char *pKey )
{
	static int s_IMEColorLookup[4] = 
	{
		COLORMIN, 
		COLORMAX,
		COLORMIN+(COLORMAX-COLORMIN)/3,
		COLORMIN+((COLORMAX-COLORMIN)*2)/3, 
	};

	unsigned int nHashID = HashStringConventional( pKey );
	nHashID &= (( 1 << 6 ) - 1 );

	int index;
	index = GET_BIT( nHashID, 0 ) | ( GET_BIT( nHashID, 5 ) << 1 );
	int r = s_IMEColorLookup[index];
	index = GET_BIT( nHashID, 1 ) | ( GET_BIT( nHashID, 4 ) << 1 );
	int g = s_IMEColorLookup[index];
	index = GET_BIT( nHashID, 2 ) | ( GET_BIT( nHashID, 3 ) << 1 );
	int b = s_IMEColorLookup[index];

	static Color s_Color;
	s_Color.SetColor( r, g, b, 255 );
	return s_Color;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextEntry::CTextEntry( CPanel2D *parent, const char * pchPanelID, const CTextInputHandlerSettings& settings )
	: CPanel2D( parent, pchPanelID ),
	m_scheduledCapsLockCheck( MAKE_SCHEDULED_FUNC( CTextEntry::OnScheduledCapsLockCheck ) )
{
	CommonConstructor( parent, pchPanelID, settings );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTextEntry::CTextEntry( CPanel2D *parent, const char * pchPanelID )
	: CPanel2D( parent, pchPanelID ),
	m_scheduledCapsLockCheck( MAKE_SCHEDULED_FUNC( CTextEntry::OnScheduledCapsLockCheck ) )
{
	CommonConstructor( parent, pchPanelID, CTextInputHandlerSettings() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextEntry::CommonConstructor( CPanel2D *parent, const char * pchPanelID, const CTextInputHandlerSettings& settings )
{
	m_bMultiline = false;
	m_settingsTextInput = settings;
	m_unMaxChars = k_nLocalizeMaxChars;
	SetAcceptsFocus( true );
	m_vecUniCharData.AddToTail( 0 ); // null terminate the string
	m_nCursorOffset = 0;
	m_LastMousePos.x = 0.0f;
	m_LastMousePos.y = 0.0f;
	m_CaretCoords.x = -1.0f;
	m_CaretCoords.y = -1.0f;
	m_flCaretHeight = 0.0f;
	m_bContentSizeDirty = false;
	m_bScrollableSizeDirty = false;
	m_bCaretPositionDirty = false;
	m_bAlwaysRenderCaret = false;
	m_bMayDrawOutsideBounds = true;
	m_flMaxWidthLastContentSize = 0.0f;
	m_flMaxHeightLastContentSize = 0.0f;
	m_bRaiseChangeEvents = false;
	m_bSelectionRectDirty = true;
	m_nSelectionStartIndex = -1;
	m_nSelectionEndIndex = -1;
	m_bLeftMouseIsDown = false;
	m_bUndoHistoryEnabled = false;
	m_flFocusTime = 0.0f;
	m_modeInput = k_ETextInputModeNormal;
	m_flTextXTranslate = 0.0f;
	m_flLastFinalWidthToScrollable = -1.0f;
	m_flLastFinalHeightToScrollable = -1.0f;
	m_bDisplayInput = true;
	m_bShowTextInputHandlerOnLeftMouseUp = false;
	m_bWarnOnCapsLock = false;
	m_bSelectionLocked = false;
	m_pTextInputHandler = NULL;
	m_bUTF8StringInvalid = true;
	m_pAutocompleteMenu = NULL;

	m_bAllowRawPlaceholderText = false;
	m_pPlaceholderText = NULL;
	m_bDeferShowKeyboard = false;
	m_bContextMenuEnabled = true;

#if defined( SOURCE2_PANORAMA )
	// IME
	m_bIMEWideCursor = false;
	m_nIMECompositionCursor = 0;
	m_IMECompositionString.AddToTail( 0 );
	m_nIMEStartingCursorInsertionOffset = 0;
	m_nIMEEndingCursorInsertionOffset = 0;
	m_bIMERejectBackspace = false;

	m_IMELoggingChannel = LOG_GENERAL;

	const char *pszID = GetID();
	bool bHasID = ( pszID && pszID[0] != '\0' );
	m_pIMEControls = new CTextEntryIMEControls( this, bHasID ? CFmtStr( "%sIMEControls", pszID ).String() : NULL );
#endif

	if( !UIEngine()->BHaveEventHandlersRegisteredForType( CTextEntry::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( TextEntryShowTextInputHandler(), &CTextEntry::OnTextEntryShowTextInputHandler );
		RegisterEventHandlerOnPanelType( TextEntryHideTextInputHandler(), &CTextEntry::OnTextEntryHideTextInputHandler );
		RegisterEventHandlerOnPanelType( Activated(), &CTextEntry::EventActivated );
		RegisterEventHandlerOnPanelType( TextEntryScrollToCursor(), &CTextEntry::OnTextEntryScrollToCursor );
		RegisterEventHandlerOnPanelType( TextEntryInsertFromClipboard(), &CTextEntry::OnInsertFromClipboard );
		RegisterEventHandlerOnPanelType( TextEntryCutToClipboard(), &CTextEntry::OnCutToClipboard );
		RegisterEventHandlerOnPanelType( TextEntryCopyToClipboard(), &CTextEntry::OnCopyToClipboard );
		RegisterEventHandlerOnPanelType( InputFocusTopLevelChanged(), &CTextEntry::EventInputFocusTopLevelChanged );
		RegisterEventHandlerOnPanelType( TextInputFinished(), &CTextEntry::HandleTextInputFinished );
		RegisterEventHandlerOnPanelType( InputFocusSet(), &CTextEntry::EventInputFocusSet );
		RegisterEventHandlerOnPanelType( InputFocusLost(), &CTextEntry::EventInputFocusLost );
		RegisterEventHandlerOnPanelType( PropertyTransitionEnd(), &CTextEntry::EventPropertyTransitionEnd );
		RegisterEventHandlerOnPanelType( TextEntryUpdate(), &CTextEntry::EventTextEntryUpdate );
		RegisterEventHandlerOnPanelType( TextEntrySetText(), &CTextEntry::EventSetText );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextEntry::~CTextEntry()
{
	if ( m_pTextInputHandler.Get() )
	{
		delete m_pTextInputHandler.Get();
	}

	FOR_EACH_VEC( m_vecUndoStack, i )
	{
		delete[] m_vecUndoStack[i];
	}
	m_vecUndoStack.RemoveAll();

	FOR_EACH_VEC( m_vecRedoStack, i )
	{
		delete[] m_vecRedoStack[i];
	}
	m_vecRedoStack.RemoveAll();

	if( !m_bDisplayInput )
	{
		FOR_EACH_VEC( m_vecUniCharData, i )
		{
			m_vecUniCharData[i] = 0;
		}
	}

	if ( m_pAutocompleteMenu.Get() )
	{
		CTextEntryAutocomplete *pMenu = m_pAutocompleteMenu.Get();
		if ( pMenu )
		{
			delete pMenu;
			m_pAutocompleteMenu = NULL;
		}
	}

#if defined( SOURCE2_PANORAMA )
	if ( m_pIMEControls.Get() )
	{
		CTextEntryIMEControls *pIMEControls = m_pIMEControls.Get();
		if ( pIMEControls )
		{
			delete pIMEControls;
			m_pIMEControls = NULL;
		}
	}
#endif // defined( SOURCE2_PANORAMA )
}


//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CTextEntry::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "text", PANORAMA_DELEGATE( &CTextEntry::PchGetText ), PANORAMA_DELEGATE( &CTextEntry::SetText ) );

	RegisterJSMethod( "SetMaxChars", PANORAMA_DELEGATE( &CTextEntry::SetMaxChars ) );
	RegisterJSMethod( "GetMaxCharCount", PANORAMA_DELEGATE( &CTextEntry::GetMaxCharCount ) );
	RegisterJSMethod( "GetCursorOffset", PANORAMA_DELEGATE( &CTextEntry::GetCursorOffset ) );
	RegisterJSMethod( "SetCursorOffset", PANORAMA_DELEGATE( &CTextEntry::SetCursorOffset ) );
	RegisterJSMethod( "ClearSelection", PANORAMA_DELEGATE( &CTextEntry::ClearSelection ) );
	RegisterJSMethod( "SelectAll", PANORAMA_DELEGATE( &CTextEntry::SelectAll ) );
	RegisterJSMethod( "RaiseChangeEvents", PANORAMA_DELEGATE( &CTextEntry::RaiseChangeEvents ) );
}


//-----------------------------------------------------------------------------
// Purpose: Update selection range after cursor movement
//-----------------------------------------------------------------------------
void CTextEntry::UpdateSelectionToInclude( int32 unPreviousCursor, int32 unNewCursorPos )
{
	if ( m_bSelectionLocked )
		return;

	int nDistanceFromStart = abs( m_nSelectionStartIndex - unPreviousCursor );
	int nDistanceFromEnd = abs( m_nSelectionEndIndex - unPreviousCursor );

	if ( nDistanceFromEnd < nDistanceFromStart )
	{
		m_nSelectionEndIndex = unNewCursorPos;
		if ( m_nSelectionStartIndex == -1 )
			m_nSelectionStartIndex = unPreviousCursor;
	}
	else
	{
		m_nSelectionStartIndex = unNewCursorPos;
		if ( m_nSelectionEndIndex == -1 )
			m_nSelectionEndIndex = unPreviousCursor;
	}

	if ( m_nSelectionStartIndex == m_nSelectionEndIndex )
	{
		m_nSelectionEndIndex = -1;
		m_nSelectionStartIndex = -1;
	}

	m_bSelectionRectDirty = true;
}


//-----------------------------------------------------------------------------
// Purpose: Delete selected text
//-----------------------------------------------------------------------------
void CTextEntry::DeleteSelection( bool bDontPushUndoHistory )
{
	if ( !bDontPushUndoHistory )
		PushUndoStack();

	if( m_nSelectionEndIndex < m_nSelectionStartIndex )
		m_vecUniCharData.RemoveMultiple( m_nSelectionEndIndex, m_nSelectionStartIndex-m_nSelectionEndIndex );
	else
		m_vecUniCharData.RemoveMultiple( m_nSelectionStartIndex, m_nSelectionEndIndex-m_nSelectionStartIndex );
	m_bUTF8StringInvalid = true;

	m_nCursorOffset = MIN( m_nSelectionEndIndex, m_nSelectionStartIndex );
	if ( m_nCursorOffset > m_vecUniCharData.Count()-1 )
		m_nCursorOffset = m_vecUniCharData.Count()-1;
	
	m_nSelectionStartIndex = -1;
	m_nSelectionEndIndex = -1;
	m_bSelectionRectDirty = true;
	m_bCaretPositionDirty = true;
	m_bContentSizeDirty = true;
	InvalidateSizeAndPosition();

	RaiseTextChangedEvent();
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether caret always render
//-----------------------------------------------------------------------------
void CTextEntry::SetAlwaysRenderCaret( bool bAlwaysRenderCaret )
{
	if ( m_bAlwaysRenderCaret != bAlwaysRenderCaret )
	{
		m_bAlwaysRenderCaret = true;
		m_bCaretPositionDirty = true;
		InvalidatePosition();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Clear selection region
//-----------------------------------------------------------------------------
void CTextEntry::ClearSelection()
{
	if ( !m_bSelectionLocked )
	{
		m_nSelectionStartIndex = -1;
		m_nSelectionEndIndex = -1;
		m_bSelectionRectDirty = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Enable/disable undo history
//-----------------------------------------------------------------------------
void CTextEntry::ClearUndoHistory()
{
	FOR_EACH_VEC( m_vecUndoStack, i )
	{
		delete[] m_vecUndoStack[i];
	}
	m_vecUndoStack.RemoveAll();

	FOR_EACH_VEC( m_vecRedoStack, i )
	{
		delete[] m_vecRedoStack[i];
	}
	m_vecRedoStack.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Enable/disable undo history
//-----------------------------------------------------------------------------
void CTextEntry::SetUndoHistoryEnabled( bool bEnabled )
{
	if ( bEnabled )
		m_bUndoHistoryEnabled = true;
	else
	{
		m_bUndoHistoryEnabled = false;
		FOR_EACH_VEC( m_vecUndoStack, i )
		{
			delete[] m_vecUndoStack[i];
		}
		m_vecUndoStack.RemoveAll();

		FOR_EACH_VEC( m_vecRedoStack, i )
		{
			delete[] m_vecRedoStack[i];
		}
		m_vecRedoStack.RemoveAll();

	}

}


//-----------------------------------------------------------------------------
// Purpose: Restore to last pushed undo state
//-----------------------------------------------------------------------------
void CTextEntry::Undo()
{
	if ( m_vecUndoStack.Count() == 0 )
		return;

	PushRedoStack();

	int iValue = m_vecUndoStack.Count()-1;
	uchar32 *pch32Restore = m_vecUndoStack[iValue];
	
	m_bUTF8StringInvalid = true;
	m_vecUniCharData.RemoveAll();
	m_vecUniCharData.AddMultipleToTail( V_strlen32( pch32Restore )+1, pch32Restore );

	delete pch32Restore;
	m_vecUndoStack.Remove( iValue );

	ClearSelection();
	if ( m_nCursorOffset > m_vecUniCharData.Count()-1 )
		m_nCursorOffset = m_vecUniCharData.Count()-1;

	m_bContentSizeDirty = true;
	m_bCaretPositionDirty = true;

	InvalidateSizeAndPosition();
}

//-----------------------------------------------------------------------------
// Purpose: Restore to last pushed redo state
//-----------------------------------------------------------------------------
void CTextEntry::Redo()
{
	if ( m_vecRedoStack.Count() == 0 )
		return;

	PushUndoStack();

	int iValue = m_vecRedoStack.Count()-1;
	uchar32 *pch32Restore = m_vecRedoStack[iValue];

	m_bUTF8StringInvalid = true;
	m_vecUniCharData.RemoveAll();
	m_vecUniCharData.AddMultipleToTail( V_strlen32( pch32Restore )+1, pch32Restore );

	delete pch32Restore;
	m_vecRedoStack.Remove( iValue );

	ClearSelection();
	if ( m_nCursorOffset > m_vecUniCharData.Count()-1 )
		m_nCursorOffset = m_vecUniCharData.Count()-1;

	m_bContentSizeDirty = true;
	m_bCaretPositionDirty = true;
	InvalidateSizeAndPosition();
}

//-----------------------------------------------------------------------------
// Purpose: Push current state to undo stack right before a modification
//-----------------------------------------------------------------------------
void CTextEntry::PushUndoStack()
{
	if ( !m_bUndoHistoryEnabled )
		return;

	uchar32 *pText = new uchar32[ m_vecUniCharData.Count() + 1 ];
	V_memcpy( pText, m_vecUniCharData.Base(), m_vecUniCharData.Count()*sizeof(uchar32) );
	pText[m_vecUniCharData.Count()] = 0;

	m_vecUndoStack.AddToTail( pText );

	if ( m_vecUndoStack.Count() > k_unUndoStackMax )
	{
		delete[] m_vecUndoStack[0];
		m_vecUndoStack.Remove( 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Push current state to redo stack right before a modification
//-----------------------------------------------------------------------------
void CTextEntry::PushRedoStack()
{
	if ( !m_bUndoHistoryEnabled )
		return;

	uchar32 *pText = new uchar32[ m_vecUniCharData.Count() ];
	V_memcpy( pText, m_vecUniCharData.Base(), m_vecUniCharData.Count()*sizeof(uchar32) );
	m_vecRedoStack.AddToTail( pText );

	if ( m_vecRedoStack.Count() > k_unUndoStackMax )
	{
		delete[] m_vecRedoStack[0];
		m_vecRedoStack.Remove( 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: place all current text in the selection set
//-----------------------------------------------------------------------------
void CTextEntry::SelectAll()
{
	if ( !m_bSelectionLocked )
	{
		m_nSelectionStartIndex = 0;
		m_nSelectionEndIndex = m_vecUniCharData.Count()-1;
		m_bContentSizeDirty = true;
		m_bSelectionRectDirty = true;
		InvalidateSizeAndPosition();
	}
}


//-----------------------------------------------------------------------------
// Purpose: watch for key presses
//-----------------------------------------------------------------------------
bool CTextEntry::OnKeyDown( const KeyData_t &code )
{
	int nOriginalCursor = m_nCursorOffset;

#if defined( SOURCE2_PANORAMA )
	if ( m_bIMERejectBackspace && code.m_KeyCode != KEY_BACKSPACE )
	{
		Log_Detailed( m_IMELoggingChannel, "KEY_BACKSPACE: Clearing state due to %d\n", code.m_KeyCode );

		// Only reject if it was the next expected character
		m_bIMERejectBackspace = false;
	}
#endif // defined( SOURCE2_PANORAMA )

	switch ( code.m_KeyCode )
	{
    default:
        break;
	case KEY_TAB:
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11: 
	case KEY_F12:
	case KEY_VOLUME_MUTE:
	case KEY_VOLUME_DOWN:
	case KEY_VOLUME_UP:
	case KEY_MEDIA_NEXT_TRACK:
	case KEY_MEDIA_PREV_TRACK:
	case KEY_MEDIA_STOP:
	case KEY_MEDIA_PLAY_PAUSE:
		return false;

	case KEY_ESCAPE:
		if ( !m_bSelectionLocked )
		{
			m_nSelectionStartIndex = -1;
			m_nSelectionEndIndex = -1;
			m_bContentSizeDirty = true;
			m_bSelectionRectDirty = true;
			InvalidateSizeAndPosition();
		}

		return false;

	case KEY_A:
		if ( IsControlShortcutPressed( code.m_Modifiers ) )
		{
			SelectAll();
			return true;
		}
		break;

	case KEY_C:
	case KEY_X:
		if ( IsControlShortcutPressed( code.m_Modifiers ) )
		{
			if ( m_nSelectionEndIndex != -1 && m_nSelectionStartIndex != -1 )
			{
				CopyToClipboard();

				// If it was X, also delete now that we've copied
				if ( code.m_KeyCode == KEY_X )
					DeleteSelection( false );

				return true;
			}
			return false;
		}
		break;

	case KEY_V:
		if ( IsControlShortcutPressed( code.m_Modifiers ) )
		{
			InsertFromClipboard();
			return true;
		}
		break;

	case KEY_Z:
		if ( IsControlShortcutPressed( code.m_Modifiers ) )
		{
			Undo();
			return true;
		}
		break;

	case KEY_Y:
		if ( IsControlShortcutPressed( code.m_Modifiers ) )
		{
			Redo();
			return true;
		}
		break;
	case KEY_UP:
	case KEY_DOWN:
		{
			bool bWrap;
			AccessStyle()->GetWhitespaceWrap( bWrap );
			if ( !bWrap )
				return false;

			// Need to hit test a line above the cursor to determine position to move to
			float flWidth = GetActualLayoutWidth();
			float flHeight = GetActualRenderHeight();

			float flLeft, flTop, flRight, flBottom;
			AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

			IUITextLayout *pLayout = CreateTextLayout( flWidth - flLeft - flRight, flHeight - flTop - flBottom );

			if ( pLayout )
			{
				float x = m_CaretCoords.x - flLeft;
				float y = m_CaretCoords.y - flTop + ( m_flCaretHeight * (code.m_KeyCode == KEY_UP ? -0.4f : 1.4f) );

				uint32 unOffset = 0;
				bool bTrailing = false;
				bool bInText = false ;
				pLayout->HitTestPoint( Vector2D( x+m_flTextXTranslate, y ), unOffset, bTrailing, bInText );

				if ( bTrailing && unOffset < (uint32)m_vecUniCharData.Count()-1 )
					++unOffset;

				if ( unOffset < (uint32)m_vecUniCharData.Count()-1 )
				{
					m_nCursorOffset = unOffset;
				
					// If shift was held, expand selection
					if ( IsShiftPressed( code.m_Modifiers ) && nOriginalCursor != m_nCursorOffset )
						UpdateSelectionToInclude( nOriginalCursor, m_nCursorOffset );
					else if ( !IsShiftPressed( code.m_Modifiers ) )
						ClearSelection();

					m_bCaretPositionDirty = true;
					InvalidatePosition();
				}

				UIEngine()->FreeTextLayout( pLayout );
			}	

			return true;
		}
		break;
	case KEY_LEFT:
		if ( IsControlPressed( code.m_Modifiers ) || ( IsOSX() && IsAltPressed( code.m_Modifiers) ) )
		{
			// Don't handle and let bubble to higher level events in empty text entries
			if ( m_vecUniCharData.Count() == 0  || IsShiftPressed( code.m_Modifiers ) )
				return false;

			if( k_ETextInputModePassword == m_modeInput )
			{
				m_nCursorOffset = 0;
			}
			else if ( m_nCursorOffset > 0 )
			{
				bool bPassedChar = false;
				while ( 1 )
				{
					m_nCursorOffset--;

					if ( !V_iswspace32( m_vecUniCharData[m_nCursorOffset] ) )
						bPassedChar = true;

					if ( m_nCursorOffset < 1 || (bPassedChar && V_iswspace32( m_vecUniCharData[m_nCursorOffset-1] ) )  )
						break;
				}
			}
		}
		else
		{
			if ( m_nCursorOffset > 0 ) 
				m_nCursorOffset--;
			else
				return false;
		}

		// If shift was held, expand selection
		if ( IsShiftPressed( code.m_Modifiers ) && nOriginalCursor != m_nCursorOffset )
			UpdateSelectionToInclude( nOriginalCursor, m_nCursorOffset );
		else if ( !IsShiftPressed( code.m_Modifiers ) )
			ClearSelection();

		m_bCaretPositionDirty = true;
		InvalidatePosition();
		return true;
		break;

	case KEY_RIGHT:
		if ( IsControlPressed( code.m_Modifiers ) || ( IsOSX() && IsAltPressed( code.m_Modifiers) ) )
		{
			// Don't handle and let bubble to higher level events in empty text entries
			if ( m_vecUniCharData.Count() == 0  || IsShiftPressed( code.m_Modifiers ) )
				return false;

			if ( k_ETextInputModePassword == m_modeInput )
			{
				m_nCursorOffset = m_vecUniCharData.Count()-1;
			}
			else if ( m_nCursorOffset < m_vecUniCharData.Count()-1 )
			{
				bool bPassedSpace = false;
				while( 1 )
				{
					if ( V_iswspace32( m_vecUniCharData[m_nCursorOffset] ) )
						bPassedSpace = true;

					m_nCursorOffset++;
					
					if ( (bPassedSpace && !V_iswspace32( m_vecUniCharData[m_nCursorOffset] ) ) || m_nCursorOffset >= m_vecUniCharData.Count()-1 )
						break;
				} 
			}
		}
		else
		{
			if ( m_nCursorOffset < m_vecUniCharData.Count()-1 )
				m_nCursorOffset++;
		}

		// If shift was held, expand selection
		if ( IsShiftPressed( code.m_Modifiers ) && nOriginalCursor != m_nCursorOffset )
			UpdateSelectionToInclude( nOriginalCursor, m_nCursorOffset );
		else if ( !IsShiftPressed( code.m_Modifiers ) )
			ClearSelection();

		m_bCaretPositionDirty = true;
		InvalidatePosition();
		return true;
		break;

	case KEY_HOME:
		m_nCursorOffset = 0;
		m_bCaretPositionDirty = true;
		InvalidatePosition();

		// If shift was held, expand selection
		if ( IsShiftPressed( code.m_Modifiers ) && nOriginalCursor != m_nCursorOffset )
			UpdateSelectionToInclude( nOriginalCursor, m_nCursorOffset );
		else
			ClearSelection();
		return true;
		break;

	case KEY_END:
		// If shift was held, expand selection
		MoveCaretToEnd( IsShiftPressed( code.m_Modifiers ) );
		return true;
		break;

	case KEY_ENTER:
	case KEY_PAD_ENTER:
		if ( IsControlPressed( code.m_Modifiers ) )
		{
			InsertCharacterAtCursor( L'\n' );			
		}
		else
		{
#if defined( SOURCE2_PANORAMA )
			// Enter should be terminating any IME completion.
			// The IME system is not reliable enough to conclude on it's own.
			// Ensure we have no orphan visual presentation. (Google IME, ii)
			// The IME system will catch up and either put it back or close as well.
			IME_RemoveInputWindow();
			IME_RemoveList();
#endif // defined( SOURCE2_PANORAMA )

			if ( BIsPanelEventSet( k_pchOnInputSubmit ) )
				DispatchPanelEvent( k_pchOnInputSubmit );
			else if ( BIsPanelEventSet( k_pchOnTextEntrySubmit ) )
				DispatchPanelEvent( k_pchOnTextEntrySubmit );
			else
				DispatchEvent( TextEntrySubmit(), this, PchGetText() );
		}
		return true;
		break;

	case KEY_BACKSPACE:
#if defined( SOURCE2_PANORAMA )
		if ( ( m_bIMEWideCursor && m_IMECompositionString.Count() > 1 ) || m_bIMERejectBackspace )
		{
			Log_Detailed( m_IMELoggingChannel, "KEY_BACKSPACE: Rejected.\n" );

			m_bIMERejectBackspace = false;
			return true;
		}

		Log_Detailed( m_IMELoggingChannel, "KEY_BACKSPACE: Accepted.\n" );
#endif // defined( SOURCE2_PANORAMA )

		if ( m_nSelectionEndIndex != -1 && m_nSelectionStartIndex != -1 )
		{
			DeleteSelection( false );
		}
		else
		{
			if ( m_nCursorOffset > 0 && m_nCursorOffset < m_vecUniCharData.Count() )
			{
				RemoveCharacter( m_nCursorOffset - 1 );
				--m_nCursorOffset;
				m_bCaretPositionDirty = true;
				InvalidatePosition();
			}
		}
		return true;
		break;

	case KEY_DELETE:
		if ( m_nSelectionEndIndex != -1 && m_nSelectionStartIndex != -1 )
		{
			DeleteSelection( false );
		}
		else
		{
			if ( m_nCursorOffset < m_vecUniCharData.Count()-1 )
			{
				RemoveCharacter( m_nCursorOffset );			
				m_bCaretPositionDirty = true;
				InvalidatePosition();
			}
		}
		return true;
		break;
	}

	if ( m_bWarnOnCapsLock && ( code.m_KeyCode == KEY_CAPSLOCK || code.m_KeyCode == KEY_CAPSLOCKTOGGLE ) )
	{
		UpdateCapsLockWarning();
	}

	// Return true to consume this event, even though it's the keytyped event that actually inserts the character.
	// This is to prevent the game handling the event.
	// The assumption is that when a textentry box is focused it should handle all key entry.
	// For example the key bound to console should actually just be used for typing and not bring up the console. 
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextEntry::MoveCaretToEnd( bool bIsShiftHeld )
{
	int nOriginalCursor = m_nCursorOffset;

	m_nCursorOffset = m_vecUniCharData.Count()-1;
	m_bCaretPositionDirty = true;
	InvalidatePosition();

	if ( bIsShiftHeld && nOriginalCursor != m_nCursorOffset )
		UpdateSelectionToInclude( nOriginalCursor, m_nCursorOffset );
	else
		ClearSelection();
}


//-----------------------------------------------------------------------------
// Purpose: Checks if symbol is a panel event
//-----------------------------------------------------------------------------
bool CTextEntry::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	if ( symProperty == k_pchOnInputSubmit || symProperty == k_pchOnTextEntrySubmit || symProperty == k_pchOnTextEntryChange )
		return true;

	return BaseClass::BIsClientPanelEvent( symProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Cut selection to clipboard
//-----------------------------------------------------------------------------
void CTextEntry::CutToClipboard()
{
	if ( m_nSelectionEndIndex != -1 && m_nSelectionStartIndex != -1 )
	{
		CopyToClipboard();
		DeleteSelection( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Copy selection to clipboard
//-----------------------------------------------------------------------------
void CTextEntry::CopyToClipboard()
{
	if ( m_nSelectionEndIndex != -1 && m_nSelectionStartIndex != -1 )
	{
		PushUndoStack();

		int nMin = MIN( m_nSelectionEndIndex, m_nSelectionStartIndex );
		int nMax = MAX( m_nSelectionStartIndex, m_nSelectionEndIndex );

		uchar32 savedchar = m_vecUniCharData[nMax];
		m_vecUniCharData[nMax] = 0;

		CStrAutoEncodeSrc2 strUTF8( m_vecUniCharData.Base()+nMin );
		UIEngine()->CopyToClipboard( strUTF8.ToString(), "" );

		m_vecUniCharData[nMax] = savedchar;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Insert clipboard contents into text entry
//-----------------------------------------------------------------------------
void CTextEntry::InsertFromClipboard()
{
	CUtlString strClipboard;
	UIEngine()->GetClipboardText( strClipboard, nullptr );
	if ( !strClipboard.IsEmpty() )
	{
		CStrAutoEncodeSrc2 strUTF32( strClipboard.Access() );
		int nChars = V_strlen32( strUTF32.ToUTF32() );
		InsertCharactersAtCursor( strUTF32.ToUTF32(), nChars );
	}
}


//-----------------------------------------------------------------------------
// Purpose: event handler
//-----------------------------------------------------------------------------
bool CTextEntry::OnInsertFromClipboard( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( ToPanel2D(pPanel.Get()) == this )
	{
		InsertFromClipboard();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: event handler
//-----------------------------------------------------------------------------
bool CTextEntry::OnCutToClipboard( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( ToPanel2D(pPanel.Get()) == this )
	{
		CutToClipboard();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: event handler
//-----------------------------------------------------------------------------
bool CTextEntry::OnCopyToClipboard( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( ToPanel2D(pPanel.Get()) == this )
	{
		CopyToClipboard();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Enable/disable context menu for cut/copy/paste
//-----------------------------------------------------------------------------
void CTextEntry::SetContextMenuEnabled( bool bEnabled )
{
	m_bContextMenuEnabled = bEnabled;
}


//-----------------------------------------------------------------------------
// Purpose: catch input
//-----------------------------------------------------------------------------
bool CTextEntry::OnKeyTyped( const KeyData_t &data )
{
	uchar32 unichar = data.m_UniChar;
	if ( unichar == 0 || unichar == '\t' )
		return false;

	if ( V_iswcntrl32( unichar ) )
		return false;

	InsertCharacterAtCursor( unichar );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Inserts one character at the cursor
//-----------------------------------------------------------------------------
void CTextEntry::InsertCharacterAtCursor( const uchar32 &unichar )
{
	InsertCharactersAtCursor( &unichar, 1 );
}


//-----------------------------------------------------------------------------
// Purpose: Check if a character is valid input given the input mode of the
//			control.
//-----------------------------------------------------------------------------
bool CTextEntry::BIsValidCharacter( const uchar32 ch32 )
{
	if ( m_modeInput == k_ETextInputModeNumeric || m_modeInput == k_ETextInputModeNumericPassword )
	{
		return V_iswdigit32( ch32 ) || ch32 == L'.' || ch32 == L'-';
	}
	else if ( m_modeInput == k_ETextInputModePhoneNumber )
	{
		return (V_iswdigit32( ch32 ) || ch32 == L'(' || ch32 == L')' || ch32 == '-');
	}
	else
	{
		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle associated daisywheel submitting, if we've set a on submit event we
// should fire it, if not let the code handle this event by bubbling it
//-----------------------------------------------------------------------------
bool CTextEntry::HandleTextInputFinished( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, bool bFinished, char const *pchText )
{
	if ( bFinished )
	{
		if ( BIsPanelEventSet( k_pchOnInputSubmit ) )
		{
			DispatchPanelEvent( k_pchOnInputSubmit );
			return true;
		}

		if ( BIsPanelEventSet( k_pchOnTextEntrySubmit ) )
		{
			DispatchPanelEvent( k_pchOnTextEntrySubmit );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: This panel or a child just received focus
//-----------------------------------------------------------------------------
bool CTextEntry::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	GetParentWindow()->TextEntryFocusChange( ptrPanel.Get() );
	ShowVRKeyboard();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: This panel or a child just lost input focus
//-----------------------------------------------------------------------------
bool CTextEntry::EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel )
{
	GetParentWindow()->TextEntryFocusChange( ptrPanel.Get() );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: When transitions end check if we need to show the VR keyboard (deferred to get final location)
//-----------------------------------------------------------------------------
bool CTextEntry::EventPropertyTransitionEnd( const CPanelPtr< IUIPanel > &ptrPanel, CStyleSymbol prop )
{
	if ( m_bDeferShowKeyboard )
	{
		ShowVRKeyboard();
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: This event exists to tell the TextEntry to refresh itself from the VR Keyboard
//-----------------------------------------------------------------------------
bool CTextEntry::EventTextEntryUpdate( const CPanelPtr< IUIPanel > &ptrPanel )
{
#if !defined( SOURCE2_PANORAMA )
	if ( this->GetParentWindow()->BIsVROverlay() )
	{
		char buffer[1024];
		vrapi::VROverlay()->GetKeyboardText( buffer, sizeof( buffer ) );
		this->SetText( buffer );
	}
#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Inserts one or more characters at the cursor
//-----------------------------------------------------------------------------
void CTextEntry::InsertCharactersAtCursor( const uchar32 *pch32, const size_t cwch )
{
	// throw away character if full
	if ( (uint32)m_vecUniCharData.Count()-1 >= m_unMaxChars )
		return;

	PushUndoStack();
	if ( m_nSelectionEndIndex != -1 && m_nSelectionStartIndex != -1 )
	{
		DeleteSelection( true );
	}

	m_bContentSizeDirty = true;
	m_bCaretPositionDirty = true;
	InvalidateSizeAndPosition();

	for ( size_t ich = 0; ich < cwch; ich++ )
	{
		uchar32 c = pch32[ich];
		// Map Unicode newlines, line breaks, and paragraph breaks to simple newlines
		if ( c == 0x0085 || c == 0x2028 || c == 0x2029 )
			c = L'\n';

		if ( !BIsValidCharacter( c ) )
		{
			UISoundSystem()->PlaySound( "txting_type_fail", UIPanel(), k_ESoundType_Effects, 1.0f );
			continue;
		}

		if ( (uint32)m_vecUniCharData.Count()-1 >= m_unMaxChars )
		{
			// ran out of room - stop inserting
			UISoundSystem()->PlaySound( "txting_type_fail", UIPanel(), k_ESoundType_Effects, 1.0f );
			break;
		}

		// Swallow characters that will force us to take up multiple vertical lines until we're
		// specifically tagged to support them.
		if ( !m_bMultiline && (c == L'\n' || c == L'\r') )
		{
			continue;
		}

		m_vecUniCharData.InsertAfter( m_nCursorOffset-1, c );
		m_nCursorOffset++;
	}

	m_bUTF8StringInvalid = true;
	RaiseTextChangedEvent();
}


//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------
bool CTextEntry::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symText( "text" );
	static CPanoramaSymbol symMaxChars( "maxchars" );
	static CPanoramaSymbol symUndoEnabled( "undohistory" );
	static CPanoramaSymbol symWarnCapsLock( "capslockwarn" );
	static CPanoramaSymbol symTextMode( "textmode" );
	static CPanoramaSymbol symPlaceholder( "placeholder" );
	static CPanoramaSymbol symAllowRawPlaceholder( "allowrawplaceholder" );
	static CPanoramaSymbol symMultiline( "multiline" );
	
	if ( symName == symText )
	{
		//	probably a bug; set the wchar text instead?
		m_UTF8String = pchValue;
		m_bUTF8StringInvalid = false;
		return true;
	}
	else if ( symName == symMaxChars )
	{
		SetMaxChars( V_atoi( pchValue ) );
		return true;
	}
	else if ( symName == symUndoEnabled )
	{
		pchValue = CSSHelpers::SkipSpaces( pchValue );
		if ( V_strnicmp( pchValue, "enabled", sizeof( "enabled" ) ) == 0 )
		{
			SetUndoHistoryEnabled( true );
		}
		else if ( V_strnicmp( pchValue, "disabled", sizeof( "disabled" ) ) == 0 )
		{
			SetUndoHistoryEnabled( false );
		}
		else
		{
			return false;
		}
		return true;
	}
	else if ( symName == symWarnCapsLock )
	{
		m_bWarnOnCapsLock = V_atoi( pchValue ) != 0;
		if ( m_bWarnOnCapsLock )
			OnScheduledCapsLockCheck();
		else
			UpdateCapsLockWarning();
		return true;
	}
	else if ( symName == symTextMode )
	{
		ETextInputMode_t eMode = ETextInputMode_tFromName( pchValue );
		SetMode( eMode );
		return true;
	}
	else if ( symName == symAllowRawPlaceholder )
	{
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &m_bAllowRawPlaceholderText ) )
			return false;

		SetAllowRawPlaceholderText( m_bAllowRawPlaceholderText );
		return true;
	}
	else if ( symName == symPlaceholder )
	{
		SetPlaceholderText( pchValue );
		return true;
	}
	else if ( symName == symMultiline )
	{
		m_bMultiline = false;
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bMultiline );
	}
	else if ( m_settingsTextInput.BSetProperty( symName, pchValue ) )
	{
		return true;
	}
	else
	{
		return BaseClass::BSetProperty( symName, pchValue );
	}
}

void CTextEntry::SetMultiline( bool bMultiline )
{
	m_bMultiline = bMultiline;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the maximum number of characters for a text entry
//-----------------------------------------------------------------------------
void CTextEntry::SetMaxChars( uint unMaxChars )
{
	// update control in case someone has already entered text that is too long

	// move cursor if necessary
	if ( m_nCursorOffset > (int32)unMaxChars )
		m_nCursorOffset = (int32)unMaxChars;

	if ( GetCharCount() > unMaxChars )
	{
		m_vecUniCharData.RemoveMultiple( unMaxChars, m_vecUniCharData.Count() - unMaxChars - 1 );
		m_bUTF8StringInvalid = true;
	}

	m_unMaxChars = unMaxChars;
}


//-----------------------------------------------------------------------------
// Purpose: done with styles, get our string
//-----------------------------------------------------------------------------
void CTextEntry::OnInitializedFromLayout()
{
	BaseClass::OnInitializedFromLayout();
	
	CLocStringSafePointer pLocString = UILocalize()->PchFindToken( UIPanel(), m_UTF8String, m_unMaxChars, k_eStringTruncationStyle_Rear, k_eStringTransformStyle_None, k_eStringEscapeStyle_None );
	SetText( pLocString->String() );

	m_UTF8String = "";
}


//-----------------------------------------------------------------------------
// Purpose: Set the input sub-mode (not normally used in typing mode, but
// used by the daisy wheel)
//-----------------------------------------------------------------------------
void CTextEntry::SetMode( panorama::ETextInputMode_t mode )
{
	if ( m_modeInput != mode )
	{
		m_modeInput = mode;
		m_bCaretPositionDirty = true;
		m_bContentSizeDirty = true;
		InvalidateSizeAndPosition();

		if ( mode == k_ETextInputModePassword || mode == k_ETextInputModeNumericPassword )
		{
			m_bDisplayInput = false;
		}
		else
		{
			m_bDisplayInput = true;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the string to display
//-----------------------------------------------------------------------------
void CTextEntry::SetText( const char *pchValue )
{
	CStrAutoEncodeSrc2 str( pchValue );
	const uchar32 *pch32Value = str.ToUTF32();

	if ( m_vecUniCharData.Count() )
		m_vecUniCharData.RemoveAll();

	m_bUTF8StringInvalid = true;
	m_vecUniCharData.AddMultipleToTail( MIN( (uint32)V_strlen32( pch32Value ), m_unMaxChars ), pch32Value );
	m_vecUniCharData.AddToTail( 0 ); // now put the null terminator back
	m_nCursorOffset = m_vecUniCharData.Count()-1;

	m_bCaretPositionDirty = true;
	m_bContentSizeDirty = true;
	InvalidateSizeAndPosition();
	
	m_nSelectionStartIndex = -1;
	m_nSelectionEndIndex = -1;
	m_bSelectionRectDirty = true;

	RaiseTextChangedEvent();
}

void CTextEntry::SetAllowRawPlaceholderText( bool bAllow )
{
	m_bAllowRawPlaceholderText = bAllow;

	if ( m_pPlaceholderText )
		m_pPlaceholderText->SetAllowRawText( bAllow );
}
//-----------------------------------------------------------------------------
// Purpose: Set the placeholder string to display
//-----------------------------------------------------------------------------
void CTextEntry::SetPlaceholderText( const char *pchValue )
{
	if ( m_pPlaceholderText == NULL )
	{
		m_pPlaceholderText = new CLabel( this, "PlaceholderText" );
		m_pPlaceholderText->SetHitTestEnabled( false );
		m_pPlaceholderText->SetAllowRawText( m_bAllowRawPlaceholderText );
	}

	m_pPlaceholderText->SetText( pchValue );
}

//-----------------------------------------------------------------------------
// Purpose: Create a text layout object for our text
//-----------------------------------------------------------------------------
IUITextLayout *CTextEntry::CreateTextLayout( float flWidth, float flHeight )
{
	// Compute size needed for text...
	const char * pchFontFamily = NULL;
	float flFontSize;
	EFontWeight eWeight;
	EFontStyle eStyle;
	AccessStyle()->GetFontStyle( &pchFontFamily, flFontSize, eStyle, eWeight );
	ETextAlign eAlign;
	AccessStyle()->GetTextAlign( eAlign );
	int nLetterSpacing;
	AccessStyle()->GetTextLetterSpacing( nLetterSpacing );

	float flLineHeight;
	AccessStyle()->GetLineHeight( flLineHeight );

	// If we aren't running in multiline mode, we force wrapping off and ignore
	// whatever our style says. This is not the most awesome.
	bool bWrap = false;
	if ( m_bMultiline )
		AccessStyle()->GetWhitespaceWrap( bWrap );

	ETextOverflow eTextOverflow;
	AccessStyle()->GetTextOverflow( eTextOverflow );
	bool bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;

	if ( eTextOverflow == k_ETextOverflowNoClip )
	{
		EOverflowValue eHorizontalOverflow, eVerticalOverflow;
		AccessStyle()->GetOverflow( eHorizontalOverflow, eVerticalOverflow );
		if ( eHorizontalOverflow == k_EOverflowNoClip || eHorizontalOverflow == k_EOverflowScroll )
			flWidth = k_flMaxWidthOrHeight;
		if ( eVerticalOverflow == k_EOverflowNoClip || eVerticalOverflow == k_EOverflowScroll )
			flHeight = k_flMaxWidthOrHeight;
	}

	IUITextLayout *pLayout = UIEngine()->CreateTextLayout( Pch32GetTextDisplay(), pchFontFamily, flFontSize, flLineHeight, eWeight, eStyle, eAlign, bWrap, bEllipsis, nLetterSpacing, flWidth, flHeight );
	if ( pLayout )
	{
		return pLayout;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Track last mouse position within us
//-----------------------------------------------------------------------------
void CTextEntry::OnMouseMove( float flMouseX, float flMouseY )
{
	m_LastMousePos.x = flMouseX;
	m_LastMousePos.y = flMouseY;

	// If we are dragging while the mouse is down, update selection state
	if ( m_bLeftMouseIsDown )
	{
		// Need to hit test text to determine where the click occurred
		float flWidth = GetActualLayoutWidth();
		float flHeight = GetActualRenderHeight();

		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

		bool bWrap;
		AccessStyle()->GetWhitespaceWrap( bWrap );

		ETextOverflow eTextOverflow;
		AccessStyle()->GetTextOverflow( eTextOverflow );
		bool bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;

		IUITextLayout *pLayout = NULL;
		if ( !bWrap && !bEllipsis )
			pLayout = CreateTextLayout( k_flMaxWidthOrHeight, flHeight - flTop - flBottom );
		else
			pLayout = CreateTextLayout( flWidth - flLeft - flRight, flHeight - flTop - flBottom );

		if ( pLayout )
		{
			float x = m_LastMousePos.x - GetInterpolatedXScrollOffset() - flLeft;
			float y = m_LastMousePos.y - GetInterpolatedYScrollOffset() - flTop;

			uint32 unOffset = 0;
			bool bTrailing = false;
			bool bInText = false;
			pLayout->HitTestPoint( Vector2D( x+m_flTextXTranslate, y ), unOffset, bTrailing, bInText );

			uint32 unPreviousOffset = m_nCursorOffset;

			m_nCursorOffset = unOffset;
			if ( bTrailing && m_nCursorOffset < m_vecUniCharData.Count()-1 )
				++m_nCursorOffset;
			
			UpdateSelectionToInclude( unPreviousOffset, m_nCursorOffset );

			m_bCaretPositionDirty = true;
			UIEngine()->FreeTextLayout( pLayout );
			InvalidatePosition();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle mouse button up
//-----------------------------------------------------------------------------
bool CTextEntry::OnMouseButtonUp( const MouseData_t &code )
{
	// Only interested in left clicks
	if ( code.m_MouseCode != MOUSE_LEFT )
		return BaseClass::OnMouseButtonUp( code );

	if ( m_bShowTextInputHandlerOnLeftMouseUp )
	{
		DispatchEventAsync( 0.0f, Activated(), this, k_ePanelEventSourceGamepad );
		return BaseClass::OnMouseButtonUp( code );
	}

	m_bLeftMouseIsDown = false;

	if ( !m_bSelectionLocked )
	{
		// If we never dragged, then one of these (start) should be set but
		// the other (end) -1, if either are still -1 normalize to not set.
		if ( m_nSelectionEndIndex == -1 || m_nSelectionStartIndex == -1 )
		{
			m_nSelectionStartIndex = -1;
			m_nSelectionEndIndex = -1;
			m_bSelectionRectDirty = true;
		}
	}

	// Duplicate calls of ShowKeyboard aren't a problem. Call just in case we are trying to reactivate the keyboard
	ShowVRKeyboard();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handle cursor when enabled
//-----------------------------------------------------------------------------
EMouseCursors CTextEntry::GetMouseCursor()
{ 
	if( IsEnabled() )
		return eMouseCursor_IBeam; 
	else
		return eMouseCursor_Arrow;
}


//-----------------------------------------------------------------------------
// Purpose: Handle mouse double clicks
//-----------------------------------------------------------------------------
bool CTextEntry::OnMouseButtonDoubleClick( const MouseData_t &code )
{
	// Only interested in left clicks
	if ( code.m_MouseCode != MOUSE_LEFT )
		return BaseClass::OnMouseButtonDoubleClick( code );

	// Need to hit test text to determine where the click occurred
	float flWidth = GetActualLayoutWidth();
	float flHeight = GetActualRenderHeight();

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

	bool bWrap;
	AccessStyle()->GetWhitespaceWrap( bWrap );

	ETextOverflow eTextOverflow;
	AccessStyle()->GetTextOverflow( eTextOverflow );
	bool bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;

	IUITextLayout *pLayout = NULL;
	if ( !bWrap && !bEllipsis )
		pLayout = CreateTextLayout( k_flMaxWidthOrHeight, flHeight - flTop - flBottom );
	else
		pLayout = CreateTextLayout( flWidth - flLeft - flRight, flHeight - flTop - flBottom );

	if ( pLayout )
	{
		float x = m_LastMousePos.x - GetInterpolatedXScrollOffset() - flLeft;
		float y = m_LastMousePos.y - GetInterpolatedYScrollOffset() - flTop;

		uint32 unOffset = 0;
		bool bTrailing = false;
		bool bInText = false ;
		pLayout->HitTestPoint( Vector2D( x+m_flTextXTranslate, y ), unOffset, bTrailing, bInText );

		m_nCursorOffset = unOffset;
		if ( bTrailing && m_nCursorOffset < m_vecUniCharData.Count()-1 )
			++m_nCursorOffset;

		if ( !m_bSelectionLocked )
		{
			m_nSelectionStartIndex = m_nCursorOffset;
			m_nSelectionEndIndex = m_nSelectionStartIndex;
			// select backwards to the start of this word
			while ( !V_isbreakablewspace32( m_vecUniCharData[m_nSelectionStartIndex] ) && m_nSelectionStartIndex > 0 )
				m_nSelectionStartIndex--;

			// we selected up to a space so move one forward
			if ( m_nSelectionStartIndex != 0 )
				m_nSelectionStartIndex++;
			// select up to the end of the next word
			while ( !V_isbreakablewspace32( m_vecUniCharData[m_nSelectionEndIndex] ) && m_nSelectionEndIndex <  m_vecUniCharData.Count()-1 )
				m_nSelectionEndIndex++;

			m_nCursorOffset = m_nSelectionEndIndex;
			m_bSelectionRectDirty = true;
		}

		m_bCaretPositionDirty = true;
		InvalidatePosition();

		UIEngine()->FreeTextLayout( pLayout );
	}


	// If we already have focus, then we are done, if we didn't have focus, bubble up the event
	// so baseclass/input layer will set us to focus as well.
	if ( BHasKeyFocus() )
	{
		return true;
	}
	else
	{
		return BaseClass::OnMouseButtonDoubleClick( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle mouse triple clicks
//-----------------------------------------------------------------------------
bool CTextEntry::OnMouseButtonTripleClick( const MouseData_t &code )
{
	// Only interested in left clicks
	if ( code.m_MouseCode != MOUSE_LEFT )
		return BaseClass::OnMouseButtonDoubleClick( code );

	SelectAll();
	m_nCursorOffset = m_nSelectionEndIndex;

	// If we already have focus, then we are done, if we didn't have focus, bubble up the event
	// so baseclass/input layer will set us to focus as well.
	if ( BHasKeyFocus() )
	{
		return true;
	}
	else
	{
		return BaseClass::OnMouseButtonDoubleClick( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle mouse clicks
//-----------------------------------------------------------------------------
bool CTextEntry::OnMouseButtonDown( const MouseData_t &code )
{
	if ( m_bContextMenuEnabled && (code.m_MouseCode == MOUSE_RIGHT) )
	{
		CSimpleContextMenu *pContextMenu = new CSimpleContextMenu( GetParentWindow(), "LabelRightClickContextMenu", this );
		if ( m_nSelectionStartIndex != -1 && m_nSelectionEndIndex != -1 )
		{
			pContextMenu->AddMenuItem( "#UI_TextEntry_CutToClipboard", "TextEntryCutToClipboard()" );
			pContextMenu->AddMenuItem( "#UI_TextEntry_CopyToClipboard", "TextEntryCopyToClipboard()" );
		}
		
		pContextMenu->AddMenuItem( "#UI_TextEntry_PasteClipboard", "TextEntryInsertFromClipboard()" );
		pContextMenu->SetVisible( true );
		pContextMenu->SetFocus();
		return true;
	}

	// Only interested in left clicks after this point
	if ( code.m_MouseCode != MOUSE_LEFT )
		return BaseClass::OnMouseButtonDown( code );

	m_bLeftMouseIsDown = true;

	if ( !m_bSelectionLocked )
	{
		m_nSelectionStartIndex = -1;
		m_nSelectionEndIndex = -1;
	}

	// Need to hit test text to determine where the click occurred
	float flWidth = GetActualLayoutWidth();
	float flHeight = GetActualRenderHeight();

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

	bool bWrap;
	AccessStyle()->GetWhitespaceWrap( bWrap );

	ETextOverflow eTextOverflow;
	AccessStyle()->GetTextOverflow( eTextOverflow );
	bool bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;

	IUITextLayout *pLayout = NULL;
	if ( !bWrap && !bEllipsis )
		pLayout = CreateTextLayout( k_flMaxWidthOrHeight, flHeight - flTop - flBottom );
	else
		pLayout = CreateTextLayout( flWidth - flLeft - flRight, flHeight - flTop - flBottom );

	if ( pLayout )
	{
		float x = m_LastMousePos.x - GetInterpolatedXScrollOffset() - flLeft;
		float y = m_LastMousePos.y - GetInterpolatedYScrollOffset()  - flTop;

		uint32 unOffset = 0;
		bool bTrailing = false;
		bool bInText = false ;
		pLayout->HitTestPoint( Vector2D( x+m_flTextXTranslate, y ), unOffset, bTrailing, bInText );

		m_nCursorOffset = unOffset;
		if ( bTrailing && m_nCursorOffset < m_vecUniCharData.Count()-1 )
			++m_nCursorOffset;

		if ( !m_bSelectionLocked )
		{
			m_nSelectionStartIndex = m_nCursorOffset;
			m_bSelectionRectDirty = true;
		}
		m_bCaretPositionDirty = true;
		InvalidatePosition();

		UIEngine()->FreeTextLayout( pLayout );
	}

#if !defined(NO_STEAM)
	// Before we do the rest...
	// Was it a simulated mouse event from the steam controller (or are we pretty sure anyway?).  If 
	// so track and return now, we let cursor pos get set above, but we'll clear the "down" flag so
	// doing selection is not allowed (weird state to enter daisywheel)
	if ( code.m_MouseCode == MOUSE_LEFT && UIInputEngine()->BIsFingerDownOnSteamControllerRightPad() )
	{
		m_bLeftMouseIsDown = false;
		m_bShowTextInputHandlerOnLeftMouseUp = true;
		return BaseClass::OnMouseButtonDown( code );
	}
#endif

	// If we already have focus, then we are done, if we didn't have focus, bubble up the event
	// so baseclass/input layer will set us to focus as well.
	if ( BHasKeyFocus() )
	{
		return true;
	}
	else
	{
		return BaseClass::OnMouseButtonDown( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel is measured
//-----------------------------------------------------------------------------
void CTextEntry::OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	VPROF_BUDGET_DETAILED( "CTextEntry::OnContentSizeTraverse", VPROF_BUDGETGROUP_TENFOOT );
	// save off the content size in case we don't re-calc this time
	float flPrevContentWidth = GetContentWidth();
	float flPrevContentHeight = GetContentHeight();

	BaseClass::OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );

	if ( bFinalDimensions || flMaxHeight != m_flMaxHeightLastContentSize || flMaxWidth != m_flMaxWidthLastContentSize || m_bContentSizeDirty )
	{
		// if not calculating final dimensions, we are updating content size
		if ( !bFinalDimensions )
		{
			m_bContentSizeDirty = false;
			m_flMaxWidthLastContentSize = flMaxWidth;
			m_flMaxHeightLastContentSize = flMaxHeight;
			m_bScrollableSizeDirty = true;
		}
		
		// include padding
		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( flMaxWidth, flMaxHeight, false, flLeft, flTop, flRight, flBottom );

		flMaxWidth = flMaxWidth - flLeft - flRight;
		flMaxHeight = flMaxHeight - flTop - flBottom;

		// Compute size needed for text...
		float flDesiredWidth = 0.0f;
		float flDesiredHeight = 0.0f;

		IUITextLayout *pLayout = CreateTextLayout( flMaxWidth, flMaxHeight );
		if ( pLayout )
		{
			pLayout->GetRequiredSize( flDesiredWidth, flDesiredHeight );
			UIEngine()->FreeTextLayout( pLayout );
		}

		// Recompute actual padding, might be smaller than we first computed if we didn't need full max region for label
		AccessStyle()->GetContentInset( flDesiredWidth, flDesiredHeight, false, flLeft, flTop, flRight, flBottom );
		*pflContentWidth = MAX( flDesiredWidth + flLeft + flRight, *pflContentWidth );
		*pflContentHeight = MAX( flDesiredHeight + flTop + flBottom, *pflContentHeight );
	}
	else
	{
		// label bounds didn't change so use last cached value
		*pflContentWidth = flPrevContentWidth;
		*pflContentHeight = flPrevContentHeight;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle styles changing
//-----------------------------------------------------------------------------
void CTextEntry::OnStylesChanged()
{
	BaseClass::OnStylesChanged();
	m_bCaretPositionDirty = true;
	m_bContentSizeDirty = true;
	m_bSelectionRectDirty = true;
}


//-----------------------------------------------------------------------------
// Purpose: Layout traverse
//-----------------------------------------------------------------------------
void CTextEntry::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	if ( flFinalWidth != GetDesiredLayoutWidth() || flFinalHeight != GetDesiredLayoutHeight() )
	{
		// our width and height were changed after calculating text height in the desired pass. Recalculating fixes drawing background and scrolling height.
		// Doesn't solve letting us grow when wrapping text and layout traverse width is less than desired width
		DesiredLayoutSizeTraverse( flFinalWidth, flFinalHeight );
	}

	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	IUITextLayout *pLayout = NULL;
	IUITextLayout *pLayoutFull = NULL;
	float flWidth = GetActualLayoutWidth();
	float flHeight = GetActualRenderHeight();

	bool bWrap;
	AccessStyle()->GetWhitespaceWrap( bWrap );

	ETextOverflow eTextOverflow;
	AccessStyle()->GetTextOverflow( eTextOverflow );
	bool bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );
	
	if ( m_bCaretPositionDirty || m_bSelectionRectDirty )
	{
		pLayout = CreateTextLayout( flWidth - flLeft - flRight, flHeight - flTop - flBottom );
		if ( !bWrap && !bEllipsis )
		{
			pLayoutFull = CreateTextLayout( k_flMaxWidthOrHeight, flHeight - flTop - flBottom );
		}
	}

	if ( m_bCaretPositionDirty )
	{
		m_bCaretPositionDirty = false;
		// Update focus time which controls flicker of caret, it moving should result in it not flickering yet
		m_flFocusTime = UIEngine()->GetCurrentFrameTime();

		if ( pLayout )
		{
			IUITextLayout::HitTestRegionRect_t rect;
			if ( !bWrap && !bEllipsis )
			{
				float flWidthNeeded, flHeightNeeded;
				pLayoutFull->GetRequiredSize( flWidthNeeded, flHeightNeeded );

				// Reset translate completely if entire string fits
				if ( flWidthNeeded <= flWidth )
					m_flTextXTranslate = 0;

				pLayoutFull->GetCharacterCoordinates( (uint32)m_nCursorOffset, rect );
			}
			else
			{
				pLayout->GetCharacterCoordinates( (uint32)m_nCursorOffset, rect );
			}
			
			m_CaretCoords.x = rect.topLeft.x + flLeft;
			m_CaretCoords.y = rect.topLeft.y + flTop;
			m_flCaretHeight = rect.bottomRight.y - rect.topLeft.y;

			if ( bWrap )
				m_flTextXTranslate = 0.0f;
			else if ( (m_CaretCoords.x + 1.0f) - m_flTextXTranslate > flFinalWidth - flLeft - flRight )
				m_flTextXTranslate = m_CaretCoords.x - (flFinalWidth - flLeft - flRight) + 1.0;
			else if ( (m_CaretCoords.x - m_flTextXTranslate) < flLeft )
				m_flTextXTranslate = m_CaretCoords.x - flLeft;

			if ( bWrap )
			{
				DispatchEventAsync( 0.01f, TextEntryScrollToCursor(), this );
			}
		}
	}

	if ( m_bSelectionRectDirty )
	{
		m_bSelectionRectDirty = false;
		m_vecSelectionRects.RemoveAll();
		if ( m_nSelectionStartIndex != -1 && m_nSelectionEndIndex != -1 )
		{
			if ( pLayoutFull )
				pLayoutFull->GetCharacterRangeCoordinates( m_nSelectionStartIndex, m_nSelectionEndIndex, m_vecSelectionRects );
			else if ( pLayout )
				pLayout->GetCharacterRangeCoordinates( m_nSelectionStartIndex, m_nSelectionEndIndex, m_vecSelectionRects );

			float flLeftFinal, flTopFinal, flRightFinal, flBottomFinal;
			AccessStyle()->GetContentInset( flFinalWidth, flFinalHeight, false, flLeftFinal, flTopFinal, flRightFinal, flBottomFinal );

			// Add in padding offsets
			FOR_EACH_VEC( m_vecSelectionRects, i )
			{
				m_vecSelectionRects[i].topLeft.x += flLeftFinal;
				m_vecSelectionRects[i].topLeft.y += flTopFinal;
				m_vecSelectionRects[i].bottomRight.x += flLeftFinal;
				m_vecSelectionRects[i].bottomRight.y += flTopFinal;
			}
		}
	}

	if ( GetActualLayoutHeight() < GetContentHeight() || GetActualLayoutWidth() < GetContentWidth() )
		m_bMayDrawOutsideBounds = true;
	else
		m_bMayDrawOutsideBounds = false;

	if ( pLayout )
		UIEngine()->FreeTextLayout( pLayout );

	if ( pLayoutFull )
		UIEngine()->FreeTextLayout( pLayoutFull );

	if ( m_pAutocompleteMenu.Get() )
		m_pAutocompleteMenu->InvalidatePosition();

#if defined( SOURCE2_PANORAMA )
	if ( m_pIMEControls.Get() )
		m_pIMEControls->InvalidatePosition();
#endif // defined( SOURCE2_PANORAMA )
}


//-----------------------------------------------------------------------------
// Purpose: Text-entry specific properties for the debugger
//-----------------------------------------------------------------------------
void CTextEntry::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	BaseClass::GetDebugPropertyInfo( pvecProperties );

	pvecProperties->AddToTail( new DebugPropertyOutput_t( "multiline", m_bMultiline ? "true" : "false" ) );
	pvecProperties->AddToTail( new DebugPropertyOutput_t( "textmode", PchNameFromETextInputMode_t( m_modeInput ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: Handler to scroll to cursor
//-----------------------------------------------------------------------------
bool CTextEntry::OnTextEntryScrollToCursor( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel )
{
	if ( ptrPanel.Get() == UIPanel() )
	{
		ScrollToFitRegion( m_CaretCoords.x, m_CaretCoords.x+1, m_CaretCoords.y - m_flCaretHeight*0.15f, 
			m_CaretCoords.y + m_flCaretHeight + m_flCaretHeight*0.15f, SCROLL_BEHAVIOR_DEFAULT, true );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Paint entry contents
//-----------------------------------------------------------------------------
void CTextEntry::Paint()
{
	VPROF_BUDGET_DETAILED( "CTextEntry::Paint", VPROF_BUDGETGROUP_TENFOOT );

	BaseClass::Paint();
	if ( m_vecUniCharData.Count() )
	{
		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( GetActualLayoutWidth(), GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

		const char * pchFontFamily;
		float flFontSize;
		EFontWeight eWeight;
		EFontStyle eStyle;
		AccessStyle()->GetFontStyle( &pchFontFamily, flFontSize, eStyle, eWeight );
		ETextAlign eAlign;
		AccessStyle()->GetTextAlign( eAlign );
		ETextDecoration eDecoration;
		AccessStyle()->GetTextDecoration( eDecoration );
		int nLetterSpacing;
		AccessStyle()->GetTextLetterSpacing( nLetterSpacing );

		float flLineHeight;
		AccessStyle()->GetLineHeight( flLineHeight );

		bool bWrap;
		AccessStyle()->GetWhitespaceWrap( bWrap );

		ETextOverflow eTextOverflow;
		AccessStyle()->GetTextOverflow( eTextOverflow );
		bool bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;

		float flDrawWidth = GetActualRenderWidth() - flRight - flLeft;
		float flDrawHeight = GetActualRenderHeight() - flBottom - flTop;

		// if we have input focus, also draw the cursor
		panorama::CPanel2D *pInputPanel = m_pTextInputHandler.Get();
		bool bDrawCursor = ( BHasKeyFocus() && GetParentWindow()->BHasFocus() ) || m_bAlwaysRenderCaret || ( pInputPanel && ( pInputPanel->BHasKeyFocus() || pInputPanel->BHasDescendantKeyFocus() ) );
		if ( bDrawCursor )
		{
			if ( m_flFocusTime == 0.0f )
				m_flFocusTime = UIEngine()->GetCurrentFrameTime();

			SetRepaint( k_EPanelRepaintFull );
		}
		else
		{
			m_flFocusTime = 0.0f;
		}

		CRenderCommandList &commandList = AccessRenderEngine()->GetCommandList();

		DrawTextRegionRenderCommand_t *pCommand = AccessRenderEngine()->DrawTextRegion( Pch32GetTextDisplay(), pchFontFamily, flFontSize, flLineHeight, eWeight, eStyle, eAlign, eDecoration, bWrap, bEllipsis, nLetterSpacing,
			flLeft - m_flTextXTranslate, flTop, GetActualRenderWidth() - flRight + m_flTextXTranslate, GetActualRenderHeight() - flBottom );

		if ( pCommand )
		{
			AccessStyle()->GetForegroundFillBrushCollectionData( pCommand->default_format.fill_brush_collection, commandList, flDrawWidth + m_flTextXTranslate, flDrawHeight );

			// Kick off async text generation job
			GetParentWindow()->AsyncAddTextRegionToCache( *pCommand );
		}

		if ( pCommand && bDrawCursor )
		{
			CPanelPtr<IUIPanel> safeptr( this );

			FillBrushCollectionWithTransition_t *pCursorColor = AccessRenderEngine()->DrawFilledRect( safeptr.GetHandleAsUInt64(), m_CaretCoords.x - m_flTextXTranslate, m_CaretCoords.y, m_CaretCoords.x + 1.0f - m_flTextXTranslate, m_CaretCoords.y + m_flCaretHeight );
			if ( pCursorColor == nullptr )
				return;

			CRenderDataListBuilder< FillBrushCollectionWithTransition_t::AnimationData_t > animationListBuilder( pCursorColor->animations, &commandList );

			FillBrushCollectionWithTransition_t::AnimationData_t *pAnimationData = animationListBuilder.AddToTail();
			pAnimationData->delay_seconds = 0.0f;
			pAnimationData->direction = k_EAnimationDirectionNormal;
			pAnimationData->fillMode = k_EAnimationFillModeNone;
			pAnimationData->duration_seconds = 1.0f;
			pAnimationData->iteration = k_flFloatInfiniteIteration;
			pAnimationData->start_time = m_flFocusTime;
			pAnimationData->timing_func = k_EAnimationEaseInOut;

			CRenderDataListBuilder< FillBrushCollectionWithTransition_t::AnimationFrameData_t > framesListBuilder( pAnimationData->frames, &commandList );

			FillBrushCollectionWithTransition_t::AnimationFrameData_t *pFrame0 = framesListBuilder.AddToTail();
			pFrame0->percent = 0.0f;
			pFrame0->timing_func = k_EAnimationEaseInOut;
			pFrame0->data.CopyFrom( pCommand->default_format.fill_brush_collection.base, commandList );

			FillBrushCollectionWithTransition_t::AnimationFrameData_t *pFrame1 = framesListBuilder.AddToTail();
			pFrame1->percent = 44.0f;
			pFrame1->timing_func = k_EAnimationEaseInOut;
			pFrame1->data.CopyFrom( pCommand->default_format.fill_brush_collection.base, commandList );

			FillBrushCollectionWithTransition_t::AnimationFrameData_t *pFrame2 = framesListBuilder.AddToTail();
			pFrame2->percent = 54.0f;
			pFrame2->timing_func = k_EAnimationEaseInOut;
			CRenderDataListBuilder< FillBrush_t > frame2FillBrushBuilder( pFrame2->data.fill_brush, &commandList );
			for ( const FillBrush_t *pFillBrush : pCommand->default_format.fill_brush_collection.base.fill_brush )
			{
				FillBrush_t *pNewFillBrush = frame2FillBrushBuilder.AddToTail();
				pNewFillBrush->CopyFrom( *pFillBrush, commandList );
				pNewFillBrush->opacity = 0.0f;
			}

			FillBrushCollectionWithTransition_t::AnimationFrameData_t *pFrame3 = framesListBuilder.AddToTail();
			pFrame3->percent = 90.0f;
			pFrame3->timing_func = k_EAnimationEaseInOut;
			CRenderDataListBuilder< FillBrush_t > frame3FillBrushBuilder( pFrame3->data.fill_brush, &commandList );
			for ( const FillBrush_t *pFillBrush : pCommand->default_format.fill_brush_collection.base.fill_brush )
			{
				FillBrush_t *pNewFillBrush = frame3FillBrushBuilder.AddToTail();
				pNewFillBrush->CopyFrom( *pFillBrush, commandList );
				pNewFillBrush->opacity = 0.0f;
			}

			FillBrushCollectionWithTransition_t::AnimationFrameData_t *pFrame4 = framesListBuilder.AddToTail();
			pFrame4->percent = 100.0f;
			pFrame4->timing_func = k_EAnimationEaseInOut;
			pFrame4->data.CopyFrom( pCommand->default_format.fill_brush_collection.base, commandList );

			// Calling GetLayoutFileDefine is mildly expensive, so only call it if we actually have selection rects
			if ( !m_vecSelectionRects.IsEmpty() )
			{
				Color selectedTextBackgroundColor( 0xFF, 0xFF, 0x00, 0x9F );
				const char *pchSelectedTextBackgroundColor = GetLayoutFileDefine( "selectedTextBackgroundColor" );
				if ( pchSelectedTextBackgroundColor )
				{
					CSSHelpers::BParseColor( &selectedTextBackgroundColor, pchSelectedTextBackgroundColor );
				}

				// Draw the selection rect
				FOR_EACH_VEC( m_vecSelectionRects, i )
				{
					IUITextLayout::HitTestRegionRect_t &rect = m_vecSelectionRects[ i ];
					AccessRenderEngine()->DrawSolidColorRect( rect.topLeft.x - m_flTextXTranslate, rect.topLeft.y, rect.bottomRight.x - m_flTextXTranslate, rect.bottomRight.y, selectedTextBackgroundColor.GetRawColor(), k_EAntialiasingNone );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get a utf-8 representation of the string
//-----------------------------------------------------------------------------
const char *CTextEntry::PchGetText() const
{
	if ( m_bUTF8StringInvalid )
	{
		m_UTF8String = CStrAutoEncodeSrc2( m_vecUniCharData.Base() ).ToString();
		m_bUTF8StringInvalid = false;
	}
	
	return m_UTF8String;
}


//-----------------------------------------------------------------------------
// Purpose: Get a utf-8 representation of the placeholder string
//-----------------------------------------------------------------------------
const char *CTextEntry::PchGetPlaceholderText() const
{
	return m_pPlaceholderText ? m_pPlaceholderText->PchGetText() : "";
}

//-----------------------------------------------------------------------------
// Purpose: Get a uchar32 representation of the string
//-----------------------------------------------------------------------------
const uchar32 *CTextEntry::Pch32GetText() const
{
	return m_vecUniCharData.Base();
}

//-----------------------------------------------------------------------------
// Purpose: Get a representation of the string for display, honoring the
// password style and text transformations
//-----------------------------------------------------------------------------
const uchar32 *CTextEntry::Pch32GetTextDisplay()
{
	ETextTransform eTransform = k_ETextTransformNone;
	AccessStyle()->GetTextTransform( eTransform );

	if ( m_bDisplayInput && eTransform == k_ETextTransformNone )
		return m_vecUniCharData.Base();

	m_vecUniCharDataDisplayFormatted.SetCount( m_vecUniCharData.Count() );

	if ( !m_bDisplayInput )
	{
		for ( uchar32 *pch32Base = m_vecUniCharDataDisplayFormatted.Base();
			pch32Base < m_vecUniCharDataDisplayFormatted.Base() + m_vecUniCharDataDisplayFormatted.Count();
			pch32Base++ )
		{
			*pch32Base = k_ch32Mask;
		}

		// ensure NUL termination (we copied over the NUL but some codepaths above may trash it)
		m_vecUniCharDataDisplayFormatted[ m_vecUniCharData.Count() - 1 ] = 0;
	}
	else
	{
		// Transform for display
		switch ( eTransform )
		{
		default:
			AssertMsg1( false, "Unknown text transform %d", eTransform );
		case k_ETextTransformUnset:
		case k_ETextTransformNone:
			// copy actual string
			m_vecUniCharDataDisplayFormatted.CopyArray( m_vecUniCharData.Base(), m_vecUniCharData.Count() );
			break;
		case k_ETextTransformUppercase:
		case k_ETextTransformLowercase:
			int nCaseFlags = eTransform == k_ETextTransformUppercase ? STRINGCASE_UPPER : STRINGCASE_LOWER;
			int nConv = V_UnicodeCaseConvert( m_vecUniCharData.Base(), m_vecUniCharDataDisplayFormatted.Base(), m_vecUniCharDataDisplayFormatted.Count(), nCaseFlags | STRINGCASE_FLAG_LINGUISTIC, (EStringConvertErrorPolicy)(STRINGCONVERT_ASSERT_REPLACE | _STRINGCONVERTFLAG_TOTALSIZE) );
			if ( nConv > m_vecUniCharDataDisplayFormatted.Count() )
			{
				m_vecUniCharDataDisplayFormatted.SetCount( nConv );
				V_UnicodeCaseConvert( m_vecUniCharData.Base(), m_vecUniCharDataDisplayFormatted.Base(), m_vecUniCharDataDisplayFormatted.Count(), nCaseFlags | STRINGCASE_FLAG_LINGUISTIC );
			}
			break;
		}
	}

	return m_vecUniCharDataDisplayFormatted.Base();
}


//-----------------------------------------------------------------------------
// Purpose: Removes a character and fires related events
//-----------------------------------------------------------------------------
void CTextEntry::RemoveCharacter( int32 offset )
{
	PushUndoStack();
	m_bUTF8StringInvalid = true;
	m_vecUniCharData.Remove( offset );
	m_bContentSizeDirty = true;
	InvalidateSizeAndPosition();
	RaiseTextChangedEvent();
}


//-----------------------------------------------------------------------------
// Purpose: Creates a text entry changed event if necessary
//-----------------------------------------------------------------------------
void CTextEntry::RaiseTextChangedEvent()
{
	// Always update focus time which controls cursor flicker
	m_flFocusTime = UIEngine()->GetCurrentFrameTime();

	SetHasClass( "HasInput", GetCharCount() > 0 );

	// only raise events when requested
	if ( m_bRaiseChangeEvents )
		DispatchEvent( TextEntryChanged(), this );

	// If the panel event is set, dispatch it
	if ( BIsPanelEventSet( k_pchOnTextEntryChange ) )
		DispatchPanelEvent( k_pchOnTextEntryChange );

}


//-----------------------------------------------------------------------------
// Purpose: Show the daisy wheel entry
//-----------------------------------------------------------------------------
bool CTextEntry::OnTextEntryShowTextInputHandler( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel )
{
	if ( ToPanel2D( ptrPanel.Get() ) == this )
	{
		// Do we already have our text entry handler open? Could happen in the case of delayed events.
		if ( !GetTextInputHandler() )
		{
			// Invoke text input handler for the text entry
			m_settingsTextInput.SetMode( m_modeInput );
		
			panorama::CreateTextInputHandler( GetParentWindow(), m_settingsTextInput, this );
		}

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTextEntry::OnTextEntryHideTextInputHandler( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel )
{
	if ( ToPanel2D( ptrPanel.Get() ) == this )
	{
		MoveCaretToEnd( false );

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the text input handler for a text entry
//-----------------------------------------------------------------------------
void CTextEntry::OnTextInputHandlerOpened( CTextInputHandler *pHandler )
{
	m_pTextInputHandler = pHandler;
	
	AddClass( "TextInputHandlerActive" );	

	m_bCaretPositionDirty = true;
	InvalidatePosition();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTextEntry::EventActivated ( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	if ( this->GetParentWindow()->BIsVROverlay() )
	{

		ShowVRKeyboard();
	}
	else if ( eSource == k_ePanelEventSourceGamepad )
	{
		OnTextEntryShowTextInputHandler( this );
	}

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: manually move the cursor pos
//-----------------------------------------------------------------------------
void CTextEntry::SetCursorOffset( int32 nCursoroffset )
{
	m_nCursorOffset = nCursoroffset;
	if ( m_nCursorOffset < 0 )
		m_nCursorOffset = 0;
	if ( m_nCursorOffset >= m_vecUniCharData.Count()  )
		m_nCursorOffset= m_vecUniCharData.Count() -1;
}


//-----------------------------------------------------------------------------
// Purpose: add or remove the capslock class if this functionality is requested
//-----------------------------------------------------------------------------
void CTextEntry::UpdateCapsLockWarning()
{
	if ( m_bWarnOnCapsLock )
	{
		bool bCapsLockEnabled = UIEngine()->UIInputEngine()->BIsCapsLockOn();
		bool bPasswordEntryFocus = IsEnabled();
		bool bEnableCapsLockWarning = ( bCapsLockEnabled && bPasswordEntryFocus );

		if ( bEnableCapsLockWarning )
		{
			AddClass( "CapsLock" );
		}
		else
		{
			RemoveClass( "CapsLock" );
		}
	}
	else if ( BHasClass( "CapsLock" ) )
	{
		RemoveClass( "CapsLock" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: check the current capslock state, catches it changing while tenfoot was alt-tabbed
//-----------------------------------------------------------------------------
void CTextEntry::OnScheduledCapsLockCheck()
{
	Assert( m_bWarnOnCapsLock == true );
	UpdateCapsLockWarning();

	m_scheduledCapsLockCheck.Schedule( 0.5f /* 500ms */ );
}


//-----------------------------------------------------------------------------
// Purpose: focusing in or out of the panel, check capslock
//-----------------------------------------------------------------------------
bool CTextEntry::EventInputFocusTopLevelChanged( CPanelPtr< IUIPanel > ptrPanel )
{
	if ( m_bWarnOnCapsLock )
	{
		UpdateCapsLockWarning();
	}

	if( BHasKeyFocus() )
	{
		CPanel2D *pNewTopmost = ToPanel2D( ptrPanel.Get() );
		bool bCurrentFocus = (pNewTopmost == this) || IsDescendantOf( pNewTopmost );
		// Ensure IME gets notified that the focus has moved to/away from the current textbox.
		// Normally this call is made from the InputFocusSet()/InputFocusLost() event handlers,
		// but when the top level inputcontext changes we don't necessarilly get
		// InputFocusSet()/InputFocusLost() events.
		if( bCurrentFocus )
		{
			GetParentWindow()->TextEntryFocusChange( UIPanel() );
		}
		else
		{
			GetParentWindow()->TextEntryFocusChange( NULL );
		}
	}

	return false;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CTextEntry::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_UTF8String );
	ValidateObj( m_vecUniCharData );
	ValidateObj( m_vecUniCharDataDisplayFormatted );
	ValidateObj( m_vecSelectionRects );
	ValidateObj( m_settingsTextInput );
	ValidateObj( m_scheduledCapsLockCheck );

	ValidateObj( m_vecUndoStack );
	FOR_EACH_VEC( m_vecUndoStack, i )
	{
		validator.ClaimArrayMemory( m_vecUndoStack[i] );
	}

	ValidateObj( m_vecRedoStack );
	FOR_EACH_VEC( m_vecRedoStack, i )
	{
		validator.ClaimArrayMemory( m_vecRedoStack[i] );
	}
	
	BaseClass::ValidateClientPanel( validator, pchName );
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
void CTextEntry::IME_SetLoggingChannel( LoggingChannelID_t loggingChannel )
{
	m_IMELoggingChannel = loggingChannel;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CTextEntry::IME_IsEnabled()
{
	return UIInputEngine()->IsIMEAllowed();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
IMEUIObjectType CTextEntry::IME_GetObjectType()
{
	return UIOT_TEXTFIELD;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
uchar32 *CTextEntry::IME_GetCompositionString()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	if ( m_IMECompositionString.Count() )
	{
		return m_IMECompositionString.Base();
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_CreateCompositionString()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	IME_ClearCompositionString();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_ClearCompositionString()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	// We can get reentered in some situations, such as:
	// IME_ClearCompositionString is called and calls RemoveCharacter.
	// RemoveCharacter raises TextChanged event.
	// Something, such as chat autocomplete, responds to TextChanged
	// in a way that triggers a new IME_ClearCompositionString.
	// Avoid this by sampling the count of composition characters
	// and then clearing the composition string so that later
	// calls will not do anything.
	// -1 because the string always has a terminal null
	int nCompositionChars = m_IMECompositionString.Count() - 1; 
	if ( nCompositionChars )
	{
		// maintain terminal null
		m_IMECompositionString.RemoveAll();
		m_IMECompositionString.AddToTail( 0 );
	}

	// remove prior characters (string has terminal null)
	for ( int i = 0; i < nCompositionChars && m_nCursorOffset > 0; i++ )
	{
		RemoveCharacter( m_nCursorOffset - 1 );
		--m_nCursorOffset;
		m_bCaretPositionDirty = true;
		InvalidatePosition();
	}

	m_nIMECompositionCursor = 0;

	m_nIMEStartingCursorInsertionOffset = GetCursorOffset();
	m_nIMEEndingCursorInsertionOffset = GetCursorOffset();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_CommitCompositionString( const uchar32 *pString )
{
	int nStringLength = pString ? V_strlen32( pString ) : 0;

	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nLen:%d\n", __FUNCTION__, nStringLength );

	if ( nStringLength )
	{
		// remove prior characters (string has terminal NULL)
		int nCompositionChars = m_IMECompositionString.Count() - 1; 
		for ( int i = 0; i < nCompositionChars && m_nCursorOffset > 0; i++ )
		{
			RemoveCharacter( m_nCursorOffset - 1 );
			--m_nCursorOffset;
			m_bCaretPositionDirty = true;
			InvalidatePosition();
		}
	}

	for ( int i = 0; i < nStringLength; i++ )
	{
		InsertCharacterAtCursor( pString[i] );
	}

	// commit has finalized 
	// maintain terminal null
	m_IMECompositionString.RemoveAll();
	m_IMECompositionString.AddToTail( 0 );
	
	IME_ClearCompositionString();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_SetCompositionStringText( const uchar32 *pString )
{
	int nStringLength = pString ? V_strlen32( pString ) : 0;

	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nLen:%d\n", __FUNCTION__, nStringLength );

	// remove prior characters (string has terminal NULL)
	int nCompositionChars = m_IMECompositionString.Count() - 1; 
	for ( int i = 0; i < nCompositionChars && m_nCursorOffset > 0; i++ )
	{
		RemoveCharacter( m_nCursorOffset - 1 );
		--m_nCursorOffset;
		m_bCaretPositionDirty = true;
		InvalidatePosition();
	}

	// For safety/clarity, since string is never expected to be of zero length on function entrance due to maintained null
	// so negative nominally not possible.
	if ( nCompositionChars > 0 )
	{
		m_bIMERejectBackspace = true;
	}
	
	m_IMECompositionString.RemoveAll();
	for ( int i = 0; i < nStringLength; i++ )
	{
		m_IMECompositionString.AddToTail( pString[i] );
		InsertCharacterAtCursor( pString[i] );
	}

	// maintain terminal null
	m_IMECompositionString.AddToTail( 0 );

	m_nIMEEndingCursorInsertionOffset = GetCursorOffset();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_SetCompositionStringPosition( uint32 nPos )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nPos:%d\n", __FUNCTION__, nPos );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_SetCursorInCompositionString( uint32 nPos )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nPos:%d\n", __FUNCTION__, nPos );

	m_nIMECompositionCursor = nPos;
	if ( m_nIMECompositionCursor < 0 )
		m_nIMECompositionCursor = 0;
	else if ( m_nIMECompositionCursor > m_IMECompositionString.Count()  )
		m_nIMECompositionCursor = m_IMECompositionString.Count();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
uint32 CTextEntry::IME_GetCaretIndex()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	return GetCursorOffset();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
uint32 CTextEntry::IME_GetBeginIndex()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	int32 nSelectionStart = GetSelectionStart();
	if ( nSelectionStart < 0 )
		nSelectionStart =  0;
	return nSelectionStart;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
uint32 CTextEntry::IME_GetEndIndex()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	int32 nSelectionEnd = GetSelectionEnd();
	if ( nSelectionEnd < 0 )
		nSelectionEnd = 0;
	return nSelectionEnd;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_ReplaceCharacters( const uchar32 *pString, uint32 nStart, uint32 nEnd )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nLen:%d, nStart:%d, nEnd:%d\n", __FUNCTION__, pString ? (int)V_strlen32( pString ) : 0, nStart, nEnd );

	ClearSelection();

	// clamp to first char
	if ( nStart == UINT_MAX )
	{
		nStart = 0;
	}

	// clamp to last char
	uint32 nCharCount = GetCharCount();
	if ( nEnd >= nCharCount )
	{
		nEnd = nCharCount ? nCharCount - 1 : 0;
	}

	if ( nCharCount )
	{
		for ( uint32 i = 0; i <= nEnd - nStart; i++ )
		{
			RemoveCharacter( nStart + i );
		}
	}

	int nStringLength = pString ? V_strlen32( pString ) : 0;
	if ( nStringLength )
	{
		SetCursorOffset( nStart );
		InsertCharactersAtCursor( pString, nStringLength );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_SetSelection( uint32 nStart, uint32 nEnd )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nStart:%d, nEnd:%d\n", __FUNCTION__, nStart, nEnd );
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_SetWideCursor( bool bWide )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: bWide:%d\n", __FUNCTION__, bWide );

	// implies a replacement policy
	m_bIMEWideCursor = bWide;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_HighlightCompositionStringText( uint32 nPos, uint32 nLen, IMETextHighlightStyle highlightStyle )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nPos:%d, nLen:%d, highlightStyle:%d\n", __FUNCTION__, nPos, nLen, highlightStyle );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_DeleteSelection()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	if ( m_nSelectionStartIndex != -1 && m_nSelectionEndIndex != -1 )
	{
		DeleteSelection( true );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_RemoveInputWindow()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	if ( m_pIMEControls.Get() )
	{
		m_pIMEControls.Get()->SetReadingString( NULL );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_DisplayInputWindow( const uchar32 *pReadingString, const IMERectF *pPosition )
{
	NOTE_UNUSED( pPosition );

	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nLen:%d\n", __FUNCTION__, pReadingString ? (int)V_strlen32( pReadingString ) : 0 );

	if ( m_pIMEControls.Get() && UIInputEngine()->IsIMEAllowed() )
	{
		m_pIMEControls.Get()->SetReadingString( pReadingString );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_RepositionInputWindow( const IMERectF *pPosition )
{
	NOTE_UNUSED( pPosition );

	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_CreateList( int nPageSize, int nListStartsAt1 )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nPageSize:%d, nListStartsAt1:%d\n", __FUNCTION__, nPageSize, nListStartsAt1 );

	if ( m_pIMEControls.Get() && UIInputEngine()->IsIMEAllowed() )
	{
		m_pIMEControls.Get()->CreateCandidateList( nPageSize, nListStartsAt1 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_RemoveList()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	if ( m_pIMEControls.Get() )
	{
		m_pIMEControls.Get()->ClearCandidateList();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_ClearList()
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );

	if ( m_pIMEControls.Get() )
	{
		m_pIMEControls.Get()->ClearCandidateList();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_ShowList( bool bShow )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: bShow:%d\n", __FUNCTION__, bShow );

	if ( m_pIMEControls.Get() && UIInputEngine()->IsIMEAllowed() )
	{
		m_pIMEControls.Get()->ShowCandidateList( bShow );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_RepositionCandidateList( const IMERectF *pPosition )
{
	NOTE_UNUSED( pPosition );

	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s:\n", __FUNCTION__ );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_SelectItemInList( int32 nItemToSelect )
{
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: nItemToSelect:%d\n", __FUNCTION__, nItemToSelect );
	
	if ( m_pIMEControls.Get()  && UIInputEngine()->IsIMEAllowed() )
	{
		m_pIMEControls.Get()->SetSelectedCandidate( nItemToSelect );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_AddToList( const uchar32 *pCandidateString )
{
	int nLength = pCandidateString ? V_strlen32( pCandidateString ) : 0;
	Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "%s: (%d Chars)\n", __FUNCTION__, nLength );

	for ( int i = 0; i < nLength; i++ )
	{
		Log_Detailed( m_IMELoggingChannel, IME_GetFunctionColor( __FUNCTION__ ), "0x%4.4x ", pCandidateString[i] );
	}
	Log_Detailed( m_IMELoggingChannel, "\n" );

	if ( m_pIMEControls.Get() && UIInputEngine()->IsIMEAllowed() )
	{
		m_pIMEControls.Get()->AddCandidate( pCandidateString );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntry::IME_SetBackspaceFilter( bool bFilterNextBackspace )
{
	m_bIMERejectBackspace = bFilterNextBackspace;
}
#endif // defined( SOURCE2_PANORAMA )

//-----------------------------------------------------------------------------
// Purpose: empty and hide the Autocomplete menu
//-----------------------------------------------------------------------------
void CTextEntry::ClearAutocomplete( void )
{
	if ( m_pAutocompleteMenu.Get() )
	{
		// Remove the children first so the Panel Focus events get called prior to adding a new Autocomplete menu. Otherwise, the focus events get called after and will change the focus to an incorrect panel.
		m_pAutocompleteMenu->RemoveAndDeleteChildren();
		m_pAutocompleteMenu->DeleteSelf();
		m_pAutocompleteMenu = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: add an autocomplete option 
//-----------------------------------------------------------------------------
void CTextEntry::AddAutocomplete( const char *pszOption )
{
	if ( !pszOption )
		return;

	CLabel *pLabel = new CLabel( this, NULL );
	pLabel->SetText( pszOption );

	AddAutocompletePanel( pLabel, pszOption );
}

//-----------------------------------------------------------------------------
// Purpose: add a CPanel2D as an autocomplete option. pszAutocompleteText will be what is placed in the text entry field if this autocomplete option is selected.
//-----------------------------------------------------------------------------
void CTextEntry::AddAutocompletePanel( CPanel2D *pPanelOption, const char* pszAutocompleteText )
{
	if ( !pPanelOption )
		return;

	CTextEntryAutocomplete *pAutocomplete = m_pAutocompleteMenu.Get();

	if ( !pAutocomplete )
	{
		const char *pszID = GetID();
		bool bHasID = (pszID && pszID[0] != '\0');
		m_pAutocompleteMenu = new CTextEntryAutocomplete( this, bHasID ? CFmtStr( "%sAutocomplete", pszID ).String() : NULL );
	}

	pPanelOption->SetAttribute( "autocompletetext", pszAutocompleteText );

	m_pAutocompleteMenu->AddOption( pPanelOption );
}

const double k_fShowKeyboardDelay = 0.05;

//-----------------------------------------------------------------------------
// Purpose: Show the VR keyboard if in VR mode when the user activates a TextEntry
//-----------------------------------------------------------------------------
void CTextEntry::ShowVRKeyboard()
{
#if !defined( SOURCE2_PANORAMA )
	if ( this->GetParentWindow()->BIsVROverlay() )
	{
		if (!m_bDeferShowKeyboard)
		{
			// First time. Init a short delay in case the focus is temporary
			m_fDeferShowKeyboardTime = UIEngine()->GetCurrentFrameTime() + k_fShowKeyboardDelay;
			m_bDeferShowKeyboard = true;
			return;
		}

		double fCurrentFrameTime = UIEngine()->GetCurrentFrameTime();
		if ( this->BHasAnyActiveTransitions() ||  fCurrentFrameTime < m_fDeferShowKeyboardTime )
		{
			// Wait until transitions finish to show the keyboard
			return;
		}

		// Do we still have focus?
		if (!this->BHasKeyFocus())
		{
			// Looks like we already lost focus, skip
			m_bDeferShowKeyboard = false;
			return;
		}

		vr::EGamepadTextInputMode unInputMode = vr::EGamepadTextInputMode::k_EGamepadTextInputModeNormal;
		bool bUseMinimalMode = false;

		const char *pchClasses = this->m_settingsTextInput.GetClasses();

		if (strstr(pchClasses, "SubmitButton") != NULL)
		{
			unInputMode = vr::EGamepadTextInputMode::k_EGamepadTextInputModeSubmit;
		}

		if (strstr(pchClasses, "Minimal") != NULL)
		{
			bUseMinimalMode = true;
		}

		/*		if ( this->BHasClass( "SubmitButton" ) )
		{
			unInputMode = vr::EGamepadTextInputMode::k_EGamepadTextInputModeSubmit;
		}*/

		m_bDeferShowKeyboard = false;

		// Set the uUserValue to a CPanelPtr so we don't crash if the panel goes away while the keyboard is up
		CPanelPtr<CPanel2D> panelPtr = CPanelPtr<CPanel2D>( this );
		// Still use minimal mode for the submit window used for chat, otherwise no minimal mode
		vrapi::VROverlay()->ShowKeyboardForOverlay( this->GetParentWindow()->GetVROverlayHandle(), unInputMode, vr::EGamepadTextInputLineMode::k_EGamepadTextInputLineModeSingleLine, "Desc", 256, PchGetText(), bUseMinimalMode, panelPtr.GetHandleAsUInt64() );
		vr::HmdRect2_t avoidRect;
		this->GetPositionWithinWindow( &avoidRect.vTopLeft.v[0], &avoidRect.vTopLeft.v[1] );

		// Overlay coordinates have (0,0) in the bottom left and go up as Y increases so we need to invert our Y values
		avoidRect.vTopLeft.v[0] = avoidRect.vTopLeft.v[0] * this->GetActualUIScaleX();
		avoidRect.vTopLeft.v[1] = this->GetParentWindow()->GetWindowHeight() - avoidRect.vTopLeft.v[1] * this->GetActualUIScaleY();
		avoidRect.vBottomRight.v[0] = avoidRect.vTopLeft.v[0] + this->GetActualLayoutWidth() * this->GetActualUIScaleX();
		avoidRect.vBottomRight.v[1] = avoidRect.vTopLeft.v[1] - this->GetActualLayoutHeight() * this->GetActualUIScaleY();

		vrapi::VROverlay()->SetKeyboardPositionForOverlay( this->GetParentWindow()->GetVROverlayHandle(), avoidRect );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Event handler to set text on the text entry
//-----------------------------------------------------------------------------
bool CTextEntry::EventSetText( const CPanelPtr< IUIPanel > &ptrPanel, const char *pchText )
{
	if ( ptrPanel.Get() != UIPanel() )
		return false;

	SetText( pchText );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextEntryAutocomplete::CTextEntryAutocomplete( CTextEntry *pParent, const char * pchPanelID ) : CPanel2D( pParent->GetParentWindow(), pchPanelID )
{
	SetAcceptsFocus( true );
 	SetLayoutFile( pParent->GetLayoutFile() );

	m_pTextEntryParent = pParent;
	PositionNearParent();
 
 	RegisterEventHandler( Activated(), this, &CTextEntryAutocomplete::EventPanelActivated );
 	RegisterForUnhandledEvent( InputFocusSet(), this, &CTextEntryAutocomplete::EventInputFocusSet );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextEntryAutocomplete::~CTextEntryAutocomplete()
{
	UnregisterForUnhandledEvent( InputFocusSet(), this, &CTextEntryAutocomplete::EventInputFocusSet );
}

//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//-----------------------------------------------------------------------------
void CTextEntryAutocomplete::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	PositionNearParent();
}

//-----------------------------------------------------------------------------
// Purpose: Sets position of panel relative to target
//-----------------------------------------------------------------------------
void CTextEntryAutocomplete::PositionNearParent()
{
	CPanel2D *pTarget = m_pTextEntryParent.Get();
	if ( !pTarget )
		return;

	float xPos = 0.0f;
	float yPos = 0.0f;
	pTarget->GetPositionWithinAncestor( NULL, &xPos, &yPos );

	// calculate available space above and below target
	float flWindowWidth, flWindowHeight;
	GetParentWindow()->GetClientDimensions( flWindowWidth, flWindowHeight );

	float flAboveTarget = yPos;
	float flBelowTarget = flWindowHeight - yPos - pTarget->GetActualRenderHeight();

	// try to position below the target. If we can't fit top or bottom, go bottom
	float flOurHeight = GetActualRenderHeight();
	if ( flBelowTarget >= flOurHeight || flOurHeight > flAboveTarget )
		yPos += pTarget->GetActualRenderHeight();
	else
		yPos -= flOurHeight;

	// if we are wider than the remaining horizontal space from where the target starts, align with right side of window
	if ( flWindowWidth - xPos < GetActualRenderWidth() )
		xPos = flWindowWidth - GetActualRenderWidth();

	CUILength lenX( xPos, CUILength::k_EUILengthLength );
	CUILength lenY( yPos, CUILength::k_EUILengthLength );

	lenX.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
	lenY.ScaleLengthValue( 1.0f / GetActualUIScaleY() );

	SetPosition( lenX, lenY, CUILength( 0.0f, CUILength::k_EUILengthLength ) );
}

//-----------------------------------------------------------------------------
// Purpose: Deletes our panel and handles fixing focus
//-----------------------------------------------------------------------------
void CTextEntryAutocomplete::DeleteSelf( bool bSetFocusToTarget )
{
	CPanel2D *pTarget = m_pTextEntryParent.Get();
	if ( pTarget )
	{
		if ( bSetFocusToTarget )
			pTarget->SetFocus();
		else if ( ToPanel2D( GetParentWindow()->UIWindowInput()->GetInputFocus() ) != pTarget )
			pTarget->RemoveStyleFlag( k_EStyleFlagFocus );
	}

	DeleteAsync();
}

//-----------------------------------------------------------------------------
// Purpose: Called when a keyboard key is pressed
//-----------------------------------------------------------------------------
bool CTextEntryAutocomplete::OnKeyDown( const KeyData_t &code )
{
	bool bHandled = BaseClass::OnKeyUp( code );
	CTextEntry *pTarget = m_pTextEntryParent.Get();

	// handle keys which make our navigation work, otherwise forward to text entry
	if ( code.m_KeyCode == KEY_ESCAPE )
	{
		DeleteSelf();
		return true;
	}
	else if ( code.m_KeyCode == KEY_UP || code.m_KeyCode == KEY_DOWN )
	{
		return bHandled;
	}
	else if ( code.m_KeyCode == KEY_TAB )
	{
		CPanel2D *pInputFocus = ToPanel2D( GetParentWindow()->UIWindowInput()->GetInputFocus() );
		SuggestionSelected( pInputFocus );
		
		return true;
	}
	else if ( code.m_KeyCode == KEY_ENTER )
	{
		// dont forward enter, however return false so default activation code is fired for selected label
		return false;
	}

	if ( pTarget )
		return pTarget->OnKeyDown( code );

	return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a keyboard key is released
//-----------------------------------------------------------------------------
bool CTextEntryAutocomplete::OnKeyUp( const KeyData_t & code )
{
	bool bHandled = BaseClass::OnKeyUp( code );

	CTextEntry *pTarget = m_pTextEntryParent.Get();
	if ( pTarget )
		return pTarget->OnKeyUp( code );

	return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a character has been entered
//-----------------------------------------------------------------------------
bool CTextEntryAutocomplete::OnKeyTyped( const KeyData_t &unichar )
{
	bool bHandled = BaseClass::OnKeyTyped( unichar );

	CTextEntry *pTarget = m_pTextEntryParent.Get();
	if ( pTarget )
		return pTarget->OnKeyTyped( unichar );

	return bHandled;
}

//-----------------------------------------------------------------------------
// Purpose: Sets target to selected suggestion and closes
//-----------------------------------------------------------------------------
void CTextEntryAutocomplete::SuggestionSelected( CPanel2D *pPanel )
{
	CTextEntry *pTarget = m_pTextEntryParent.Get();
	const char *pszAutocompleteText = pPanel->GetAttribute( "autocompletetext", "" );
	if ( *pszAutocompleteText )
	{
		pTarget->SetText( pszAutocompleteText );
	}
	else
	{
		CLabel *pLabel = (CLabel*)pPanel;
		Assert( pPanel->GetPanelType() == CLabel::GetPanelSymbol() );
		pTarget->SetText( pLabel->PchGetText() );
	}

	// we need to simulate the user submitting text, so fire the event
	DispatchEvent( TextEntrySubmit(), pTarget, pTarget->PchGetText() );

	DeleteSelf();		//DeleteSelf( false );
}

//-----------------------------------------------------------------------------
// Purpose: Add a selectable option to the dropdown
//-----------------------------------------------------------------------------
void CTextEntryAutocomplete::AddOption( CPanel2D *pPanel )
{
	// No point adding more than 100 panels, the user will have to type something to filter more specifically
	if ( GetChildCount() >= 100 )
		return;

	pPanel->SetParent( this );
	pPanel->SetSelectionPosition( k_flSelectionPosInvalid, k_flSelectionPosAuto );
	pPanel->SetAcceptsFocus( true );

	// update focus
	if ( GetChildCount() > 0 )
	{
		GetChild( 0 )->SetFocus();

		// we want the text entry to believe it has focus. We will be feeding it all key input
		m_pTextEntryParent->AddStyleFlag( k_EStyleFlagFocus );
	}
	else
	{
		DeleteSelf();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Event fired when a child panel (or ourselves) is activated
//-----------------------------------------------------------------------------
bool CTextEntryAutocomplete::EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pHitPanel = ToPanel2D( pPanel.Get() );
	if ( !pHitPanel )
		return false;

	SuggestionSelected( pHitPanel );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Event fired when input focus changes
//-----------------------------------------------------------------------------
bool CTextEntryAutocomplete::EventInputFocusSet( const CPanelPtr< IUIPanel > &pPanel )
{
	CPanel2D *pHasFocus = ToPanel2D( pPanel.Get() );

	// if panel isn't the target or in our tree, close
	if ( !pHasFocus || ( pHasFocus != m_pTextEntryParent.Get() && pHasFocus != this && !pHasFocus->IsDescendantOf( this ) ) )
		DeleteSelf( false );

	return false;
}

#if defined( SOURCE2_PANORAMA )
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextEntryIMEControls::CTextEntryIMEControls( CTextEntry *pParent, const char *pchPanelID ) : CPanel2D( pParent->GetParentWindow(), pchPanelID )
{
	m_nCandidateListPageSize = 1;
	m_nCandidateListSelectedIndex = -1;

	m_bShowCandidateList = false;

	m_pTextEntryParent = pParent;
 
	// create child controls as necessary (i.e. korean IME does not need a candidacy list)
	m_pReadingStringLabel = NULL;
	m_pCandidateList = NULL;

	SetAcceptsFocus( false );
 	SetLayoutFile( pParent->GetLayoutFile() );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextEntryIMEControls::~CTextEntryIMEControls()
{
	GetParentWindow()->TextEntryInvalid( m_pTextEntryParent.Get()->UIPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	PositionNearParent();
}

//-----------------------------------------------------------------------------
// Purpose: Sets position of panel relative to target
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::PositionNearParent()
{
	CPanel2D *pTarget = m_pTextEntryParent.Get();
	if ( !pTarget )
		return;

	float xPos = 0.0f;
	float yPos = 0.0f;
	pTarget->GetPositionWithinAncestor( NULL, &xPos, &yPos );

	// calculate available space above and below target
	float flWindowWidth, flWindowHeight;
	GetParentWindow()->GetClientDimensions( flWindowWidth, flWindowHeight );

	float flAboveTarget = yPos;
	float flBelowTarget = flWindowHeight - yPos - pTarget->GetActualRenderHeight();

	// try to position below the target. If we can't fit top or bottom, go bottom
	float flOurHeight = GetActualRenderHeight();
	if ( flBelowTarget >= flOurHeight || flOurHeight > flAboveTarget )
		yPos += pTarget->GetActualRenderHeight();
	else
		yPos -= flOurHeight;

	// if we are wider than the remaining horizontal space from where the target starts, align with right side of window
	if ( flWindowWidth - xPos < GetActualRenderWidth() )
		xPos = flWindowWidth - GetActualRenderWidth();

	CUILength lenX( xPos, CUILength::k_EUILengthLength );
	CUILength lenY( yPos, CUILength::k_EUILengthLength );

	lenX.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
	lenY.ScaleLengthValue( 1.0f / GetActualUIScaleY() );

	SetPosition( lenX, lenY, CUILength( 0.0f, CUILength::k_EUILengthLength ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::CreateCandidateList( int nPageSize, int nListStartsAt1 )
{
	NOTE_UNUSED( nListStartsAt1 );

	m_nCandidateListPageSize = nPageSize;

	ClearCandidateList();
}

//-----------------------------------------------------------------------------
// Purpose: Add a selectable option to the dropdown
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::AddCandidate( const uchar32 *pCandidateString )
{
	if ( !m_pCandidateList.Get() )
	{
		// create as necessary
		m_pCandidateList = new CPanel2D( this, NULL );
		m_pCandidateList->AddClass( "TextEntryIMECandidateList" );
		m_pCandidateList->SetAcceptsFocus( false );
		m_pCandidateList->SetVisible( m_bShowCandidateList );
	}

	int nRowNumber = m_pCandidateList->GetChildCount() + 1;

	CPanel2D *pRowParent = new CPanel2D( m_pCandidateList.Get(), NULL );
	pRowParent->AddClass( "TextEntryIMECandidateRow" );
	pRowParent->SetAcceptsFocus( false );

	CLabel *pPrefixLabel = new CLabel( pRowParent, NULL );
	pPrefixLabel->AddClass( "TextEntryIMECandidateRowPrefix" );
	pPrefixLabel->SetAcceptsFocus( false );
	pPrefixLabel->SetText( CFmtStr( "%d ", nRowNumber <= 9 ? nRowNumber : nRowNumber % m_nCandidateListPageSize ).Get() );

	CLabel *pSuffixLabel = new CLabel( pRowParent, NULL );
	pSuffixLabel->AddClass( "TextEntryIMECandidateRowSuffix" );
	pSuffixLabel->SetAcceptsFocus( false );
	pSuffixLabel->SetText( CStrAutoEncodeSrc2( pCandidateString ).ToUTF8() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::SetSelectedCandidate( int nItemToSelect )
{
	// track for later, list may not be visible or populated yet
	m_nCandidateListSelectedIndex = nItemToSelect;

	if ( m_pCandidateList.Get() )
	{
		// remove any selection
		int nChildCount = m_pCandidateList->GetChildCount();
		for ( int i = 0; i < nChildCount; i++ )
		{
			CPanel2D *pRowParent = m_pCandidateList->GetChild( i );
			if ( pRowParent )
			{
				pRowParent->RemoveClass( "Highlight" );
			}
		}

		if ( nItemToSelect != -1 && nChildCount )
		{
			// place the new selection
			nItemToSelect %= nChildCount;
			CPanel2D *pRowParent = m_pCandidateList->GetChild( nItemToSelect );
			if ( pRowParent )
			{
				pRowParent->AddClass( "Highlight" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::ClearCandidateList()
{
	// no selection
	m_nCandidateListSelectedIndex = -1;

	if ( m_pCandidateList.Get() )
	{
		m_pCandidateList->RemoveAndDeleteChildren();

		// ensure no visibility artifacts due to degenerate border
		// visibility to expected state will be re-established when items added
		m_pCandidateList->SetVisible( false );
		return;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::ShowCandidateList( bool bShow )
{
	// track state for sloppy callers (IME flow is highly variant)
	m_bShowCandidateList = bShow;

	if ( m_pCandidateList.Get() )
	{
		m_pCandidateList->SetVisible( m_bShowCandidateList );
	}

	if ( m_bShowCandidateList )
	{
		// ensure sloppy caller's out-of-order calls do the right intended thing
		SetSelectedCandidate( m_nCandidateListSelectedIndex );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTextEntryIMEControls::SetReadingString( const uchar32 *pReadingString )
{
	if ( !m_pReadingStringLabel.Get() && pReadingString && pReadingString[0] )
	{
		// create as necessary
		m_pReadingStringLabel = new CLabel( this, NULL );
		m_pReadingStringLabel->AddClass( "TextEntryIMEReadingString" );
		m_pReadingStringLabel->SetAcceptsFocus( false );
	}

	if ( m_pReadingStringLabel.Get() )
	{
		m_pReadingStringLabel->SetText( pReadingString ? CStrAutoEncodeSrc2( pReadingString ).ToUTF8() : NULL );

		// hide reading string when empty
		m_pReadingStringLabel->SetHasClass( "NoReadingString", !pReadingString || !pReadingString[0] );
	}
}

#endif // defined( SOURCE2_PANORAMA )
