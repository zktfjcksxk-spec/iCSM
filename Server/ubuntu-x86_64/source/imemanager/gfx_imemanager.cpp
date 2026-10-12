#include "vstdlib/vstrtools.h"

#include "imemanager.h"

#include "gfx_imemanager.h"
#include "gfx_imemanagerwin32.h"

IMEManagerBase::IMEManagerBase()
{
	m_bIMEEnabled = true;
	m_pUIView = NULL;
	m_pActiveComposingTextField = NULL;
	m_nCursorPosition = 0;
};

IMEManagerBase::~IMEManagerBase()
{
};

bool IMEManagerBase::Init()
{ 
	return false;
}

const uchar32 *IMEManagerBase::GetCompositionString()
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast< IIMEUITextField* >( pFocusedObject );
			return pTextField->IME_GetCompositionString();
		}
	}

	return NULL;
}

void IMEManagerBase::StartComposition()
{
	// we need to save the current position in text field
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast< IIMEUITextField* >( pFocusedObject );
			if ( pTextField->IME_IsEnabled() )
			{
				// Marks start composition (not reliable)
				// Start Composition arrives out of order or not-at-all
				m_pActiveComposingTextField = pTextField;

				// first of all, if we have an active selection - remove it
				pTextField->IME_DeleteSelection();

				int32 nBegin = pTextField->IME_GetBeginIndex();
				m_nCursorPosition = nBegin;
				pTextField->IME_SetSelection( nBegin, nBegin );
				pTextField->IME_CreateCompositionString();
			}
		}
	}
}

void IMEManagerBase::FinalizeComposition( const wchar_t *pString )
{
	int nLength = pString ? V_wcslen( pString ) : 0;

	Log_Detailed( LOG_IME, LOG_COLOR_GREEN, "IME Finalize Composition (%d Chars):", nLength );
	for ( int i = 0; i < nLength; i++ )
	{
		Log_Detailed( LOG_IME, LOG_COLOR_GREEN, "0x%4.4x ", pString[i] );
	}
	Log_Detailed( LOG_IME, "\n" );

	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			if ( m_pActiveComposingTextField )
			{
				m_pActiveComposingTextField->IME_CommitCompositionString( CStrAutoEncodeSrc2( pString ).ToUTF32() );
			}
			else
			{
				// in this case, no start composition event is sent and whole
				// text is being sent through ime_finalize message. If we don't
				// have pTextField at this time we need to find currently focused
				// one and use it.
				IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
				pTextField->IME_ReplaceCharacters( CStrAutoEncodeSrc2( pString ).ToUTF32(), pTextField->IME_GetBeginIndex(), pTextField->IME_GetEndIndex() );
			}
		}
	}
}

void IMEManagerBase::ClearComposition()
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_ClearCompositionString();
		}
	}
}


void IMEManagerBase::SetCompositionText( const wchar_t *pString )
{
	int nLength = pString ? V_wcslen( pString ) : 0;

	Log_Detailed( LOG_IME, LOG_COLOR_GREEN, "IME Composition (%d Chars):", nLength );
	for ( int i = 0; i < nLength; i++ )
	{
		Log_Detailed( LOG_IME, LOG_COLOR_GREEN, "0x%4.4x ", pString[i] );
	}
	Log_Detailed( LOG_IME, "\n" );

	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_SetCompositionStringText( CStrAutoEncodeSrc2( pString ).ToUTF32() );

			if ( ((GFxIMEManagerWin32*)this)->m_IMETag == GFxIME_Ch_Simp_QQPinyin && ((GFxIMEManagerWin32*)this)->m_OSVersion == GFxIMEManagerWin32::OSVER_WIN7 )
			{
				// This particular IME needs to hint panorama that a backspace event should be rejected
				if ( nLength > 0 )
				{
					pTextField->IME_SetBackspaceFilter( true );
				}
			}
		}
	}
}

void IMEManagerBase::SetCompositionPosition()
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			uint32 nPos = pTextField->IME_GetCaretIndex();
			pTextField->IME_SetCompositionStringPosition( nPos );
		}
	}
}

void IMEManagerBase::SetCursorInComposition( uint32 nPos )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_SetCursorInCompositionString( nPos );
		}
	}
}

void IMEManagerBase::SetWideCursor( bool bWide )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_SetWideCursor( bWide );
		}
	}
}

// Sets conversion mode. Base class version does nothing.
bool IMEManagerBase::SetConversionMode(const uint32 convMode)
{
	NOTE_UNUSED(convMode);
	return false;
}

// retrieves conversion mode. Base class version does nothing.
const char *IMEManagerBase::GetConversionMode() 
{
	return "UNKNOWN";
}

// Enables/Disables IME. 
bool IMEManagerBase::SetEnabled(bool enable) 
{
	NOTE_UNUSED(enable); 
	return false; 
}

bool IMEManagerBase::GetEnabled()
{
	return false;
} 

// Retrieves current input language
const char *IMEManagerBase::GetInputLanguage() 
{
	return "UNKNOWN";
}

// Support for OnIMEComposition event
void IMEManagerBase::BroadcastIMEConversion( const wchar_t* pString ) 
{ 
}

// Support for OnSwitchLanguage event
void IMEManagerBase::BroadcastSwitchLanguage( const wchar_t *pString )
{
}

// Support for OnSetSupportedLanguages event
void IMEManagerBase::BroadcastSetSupportedLanguages( const wchar_t *pString )
{ 
}

// Support for OnSetSupportedIMEs event
void IMEManagerBase::BroadcastSetSupportedIMEs( const wchar_t *pString )
{ 
}

// Support for OnSetCurrentInputLanguage event
void IMEManagerBase::BroadcastSetCurrentInputLanguage( const wchar_t *pString )
{
	Log_Detailed( LOG_IME, LOG_COLOR_GREEN, "Set Current Language: %s\n", CTempWStringToPrintableString( pString ).Get() );
};

// Support for OnSetIMEName event
void IMEManagerBase::BroadcastSetIMEName( const wchar_t *pString ) 
{ 
}

// Support for OnSetConversionStatus event
void IMEManagerBase::BroadcastSetConversionStatus( const wchar_t *pString )
{ 
}

// Support for OnBroadcastRemoveStatusWindow event
void IMEManagerBase::BroadcastRemoveStatusWindow()
{ 
}

// Support for OnBroadcastDisplayStatusWindow event
void IMEManagerBase::BroadcastDisplayStatusWindow()
{ 
}

// SetCompositionString
bool IMEManagerBase::SetCompositionString( const wchar_t *pCompString ) 
{
	NOTE_UNUSED(pCompString); 
	return false; 
}

void IMEManagerBase::OnShutdown()
{
}

void IMEManagerBase::GetMetrics( IMERectF *pviewRect, IMERectF *pcursorRect, int cursorOffset )
{
	NOTE_UNUSED( pcursorRect );

#if 0 //DS2
	if ( m_pActiveComposingTextField )
	{
		Render::Matrix2F wm = m_pActiveComposingTextField->GetWorldMatrix();
		RectF vr = m_pActiveComposingTextField->GetBounds(wm);
		if (pviewRect)
			*pviewRect = TwipsToPixels(vr);

		uintp curspos = m_pActiveComposingTextField->GetCompositionStringPosition();
		if (curspos == MAX_UINTP)
			curspos = m_pActiveComposingTextField->GetCaretIndex();
		else
			curspos += m_pActiveComposingTextField->GetCompositionStringLength();
		curspos += cursorOffset;
		if ((intp)curspos < 0)
			curspos = 0;

		RectF cr = m_pActiveComposingTextField->GetCursorBounds(curspos, 0, 0);

		cr = wm.EncloseTransform(cr);
		if (pcursorRect)
			*pcursorRect = TwipsToPixels(cr);
	}
#endif
}

void IMEManagerBase::HighlightText( uint32 nPos, uint32 nLen, IMETextHighlightStyle highlightStyle, bool bClause )
{
    NOTE_UNUSED( bClause );

	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_HighlightCompositionStringText( nPos, nLen, highlightStyle );
		}
	}
}

void IMEManagerBase::SetActiveUIView( IIMEUIView *pUIView )
{
	Log_Detailed( LOG_IME, LOG_COLOR_CYAN, "SetActiveUIView(): IMEUIView: 0x%8.8p\n", pUIView );

	if  ( m_pUIView && !pUIView )
	{
		// Losing Activation
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( m_pActiveComposingTextField && m_pActiveComposingTextField == pFocusedObject )
		{
			DoFinalize( true );
		}
	}

	m_pUIView = pUIView;
}

bool IMEManagerBase::IsUIViewActive( IIMEUIView *pUIView ) const
{
	return m_pUIView == pUIView;
}

IIMEUIView* IMEManagerBase::GetActiveUIView() const
{
	return m_pUIView;   
}

void IMEManagerBase::HandleMouseDownEvent( IIMEUIView *pUIView, IIMEUIObject *pItemUnderMouse )
{
	// Mouse clicks on an currently active text field need to finalize.
	// Otherwise focus changes handle.
	if ( IsUIViewActive( pUIView ) && m_pActiveComposingTextField && pItemUnderMouse == m_pActiveComposingTextField )
	{
		// if mouse clicked on the same active text field then finalize.
		// Otherwise let HandleFocus to handle the stuff.
		DoFinalize();
	}
}

void IMEManagerBase::HandleFocusChange( IIMEUIObject *pObject, bool bFocusSet )
{
	if ( !bFocusSet && m_pActiveComposingTextField && m_pActiveComposingTextField == pObject )
	{
		// Can't seem to finalize properly when focus is lost
		// As focus changes to another panorama text object, it erroneously gets the final candidate.
		// Instead finalize as a cancel operation.
		DoFinalize( true );
	}
}

IMEEventResult IMEManagerBase::HandleIMEEvent( IIMEUIView* pUIView, const IMEEvent &imeEvent )
{
	NOTE_UNUSED2( pUIView, imeEvent );
	return IME_EVENT_NOTHANDLED;
}

void IMEManagerBase::DoFinalize( bool bCancel )
{
	OnFinalize( bCancel );
	m_pActiveComposingTextField = NULL;
}

// invoked when need to finalize the composition.
void IMEManagerBase::OnFinalize( bool bCancel )
{
}

void IMEManagerBase::EnableIME( bool bEnable )
{
	if (m_bIMEEnabled != bEnable)
	{
		m_bIMEEnabled = bEnable;
		OnEnableIME( bEnable );
	}
}

// handles enabling/disabling IME, invoked from EnableIME method
void IMEManagerBase::OnEnableIME(bool enable) 
{ 
	NOTE_UNUSED(enable); 
}

void IMEManagerBase::Invoke_RemoveInputWindow()
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_RemoveInputWindow();
		}
	}
}

void IMEManagerBase::Invoke_DisplayInputWindow( const uchar32 *pReadingString, const IMERectF *pPosition )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_DisplayInputWindow( pReadingString, pPosition );
		}
	}
}

void IMEManagerBase::Invoke_RepositionInputWindow( const IMERectF *pPosition )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_RepositionInputWindow( pPosition );
		}
	}
}

void IMEManagerBase::Invoke_CreateList( int nPageSize, int nListStartsAt1 )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_CreateList( nPageSize, nListStartsAt1 );
		}
	}
}

void IMEManagerBase::Invoke_RemoveList()
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_RemoveList();
		}
	}
}

void IMEManagerBase::Invoke_ClearList()
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_ClearList();
		}
	}
}

void IMEManagerBase::Invoke_ShowList( bool bShow )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_ShowList( bShow );
		}
	}
}

void IMEManagerBase::Invoke_RepositionCandidateList( const IMERectF *pPosition )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_RepositionCandidateList( pPosition );
		}
	}
}

void IMEManagerBase::Invoke_SelectItemInList( int32 nItemToSelect )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_SelectItemInList( nItemToSelect );
		}
	}
}

void IMEManagerBase::Invoke_AddToList( const wchar_t *pCandidateString )
{
	if ( m_pUIView )
	{
		IIMEUIObject *pFocusedObject = m_pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
			pTextField->IME_AddToList( CStrAutoEncodeSrc2( pCandidateString ).ToUTF32() );
		}
	}
}
