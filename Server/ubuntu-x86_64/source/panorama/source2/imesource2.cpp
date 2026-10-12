//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"

#include "iimemanager.h"
#include "imesource2.h"
#include "panorama/controls/textentry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CIMEUITextField::CIMEUITextField()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CIMEUITextField::~CIMEUITextField()
{
	m_pTextEntry.Clear();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIMEUITextField::ClearFocusedTextEntry( panorama::CTextEntry *pTextEntry )
{
	if ( m_pTextEntry.Get() == pTextEntry )
	{
		// Don't send lost focus, abandon as quietly as possible.
		// The calling object is invalid in some manner where it should not be accessed.
		m_pTextEntry.Clear();

		// hint the IME system (which will callback here, discover the null text entry and 
		// throw the UI portion of the related calls away as intended).
		g_pIMEManager->HandleFocusChange( pTextEntry, false );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIMEUITextField::SetFocusedTextEntry( panorama::CTextEntry *pTextEntry )
{
	if ( m_pTextEntry.Get() != pTextEntry )
	{
		// Lost focus
		g_pIMEManager->HandleFocusChange( m_pTextEntry.Get(), false );
	}

	m_pTextEntry = pTextEntry;
	if ( m_pTextEntry.Get() )
	{
		m_pTextEntry.Get()->IME_SetLoggingChannel( g_pIMEManager->GetLoggingChannel() );

		// Gained focus
		g_pIMEManager->SetIMEEnabled( true );
		g_pIMEManager->HandleFocusChange( m_pTextEntry.Get(), true );
	}
	else
	{
		g_pIMEManager->SetIMEEnabled( false );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
panorama::CTextEntry *CIMEUITextField::GetFocusedTextEntry()
{
	return m_pTextEntry.Get();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CIMEUITextField::IsKeyFocused()
{
	return m_pTextEntry.Get() && m_pTextEntry.Get()->BHasKeyFocus();
}