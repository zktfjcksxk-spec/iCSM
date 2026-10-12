//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef _IMEUIINTERFACE_H_
#define _IMEUIINTERFACE_H_
#pragma once

#include "tier0/logging.h"

// styles of text highlighting (used in HighlightText)
enum IMETextHighlightStyle
{
	THS_CompositionSegment   = 0,
	THS_ClauseSegment        = 1,
	THS_ConvertedSegment     = 2,
	THS_PhraseLengthAdj      = 3,
	THS_LowConfSegment       = 4
};

enum IMEUIObjectType
{
	UIOT_UNKNOWN = 0,
    UIOT_TEXTFIELD,
};

struct IMERectF
{
	IMERectF()
	{
		m_nX = 0;
		m_nY = 0;
		m_nWidth = 0;
		m_nHeight = 0;
	}

	float m_nX;
	float m_nY;
	float m_nWidth;
	float m_nHeight;
};

class IIMEUIObject
{
public:
	virtual IMEUIObjectType IME_GetObjectType() = 0;
};

class IIMEUITextField : public IIMEUIObject
{
public:
	virtual void IME_SetLoggingChannel( LoggingChannelID_t loggingChannel ) = 0;
	virtual bool IME_IsEnabled() = 0;
	virtual IMEUIObjectType IME_GetObjectType() = 0;

	// Composition Support
	virtual uchar32 *IME_GetCompositionString() = 0;
	virtual void IME_CreateCompositionString() = 0;
	virtual void IME_ClearCompositionString() = 0;
	virtual void IME_CommitCompositionString( const uchar32 *pString ) = 0;
	virtual void IME_SetCompositionStringText( const uchar32 *pString ) = 0;
	virtual void IME_SetCompositionStringPosition( uint32 nPos ) = 0;
	virtual void IME_SetCursorInCompositionString( uint32 nPos ) = 0;
	virtual uint32 IME_GetCaretIndex() = 0;
	virtual uint32 IME_GetBeginIndex() = 0;
	virtual uint32 IME_GetEndIndex() = 0;
	virtual void IME_ReplaceCharacters( const uchar32 *pString, uint32 nStart, uint32 nEnd ) = 0;
	virtual void IME_SetSelection( uint32 nStart, uint32 nEnd ) = 0;
	virtual void IME_SetWideCursor( bool bWide ) = 0;
	virtual void IME_HighlightCompositionStringText( uint32 nPos, uint32 nLen, IMETextHighlightStyle highlightStyle ) = 0;
	virtual void IME_DeleteSelection() = 0;

	// Candidacy support
	virtual	void IME_RemoveInputWindow() = 0;
	virtual	void IME_DisplayInputWindow( const uchar32 *pReadingString, const IMERectF *pPosition ) = 0;
	virtual	void IME_RepositionInputWindow( const IMERectF *pPosition ) = 0;
	virtual	void IME_CreateList( int nPageSize, int nListStartsAt1 ) = 0;
	virtual	void IME_RemoveList() = 0;
	virtual	void IME_ClearList() = 0;
	virtual	void IME_ShowList( bool bShow ) = 0;
	virtual	void IME_RepositionCandidateList( const IMERectF *pPosition ) = 0;
	virtual	void IME_SelectItemInList( int32 nItemToSelect ) = 0;
	virtual	void IME_AddToList( const uchar32 *pCandidateString ) = 0;

	// Composition Fixup
	virtual void IME_SetBackspaceFilter( bool bFilterNextBackspace ) = 0;
};

class IIMEUIView
{
public:
	// Get IME aware focused object
	virtual IIMEUIObject *GetFocusedObject() = 0;
	virtual PlatWindow_t GetAssociatedPlatWindow() = 0;
};

#endif
