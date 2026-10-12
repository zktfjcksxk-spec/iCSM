//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//
//=====================================================================================//
#include "cbase.h"
#include "c_world_vgui_text.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_WorldVguiText, DT_WorldVguiText, CWorldVguiText )
	RecvPropBool( RECVINFO( m_bEnabled ) ),
	RecvPropString( RECVINFO(m_szDisplayText) ),
	RecvPropString( RECVINFO(m_szDisplayTextOption) ),
	RecvPropString( RECVINFO(m_szFont) ),
	RecvPropInt( RECVINFO(m_iTextPanelWidth) ),
	RecvPropInt( RECVINFO(m_clrText), 0, RecvProxy_Int32ToColor32 ),
END_RECV_TABLE()

C_WorldVguiText::C_WorldVguiText()
{
}

C_WorldVguiText::~C_WorldVguiText()
{
}

//-----------------------------------------------------------------------------
// Purpose: Recieve a message from the server
//-----------------------------------------------------------------------------
void C_WorldVguiText::ReceiveMessage( int classID, bf_read &msg )
{
	// Make sure our IDs match
	if ( classID != GetClientClass()->m_ClassID )
	{
		// Message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}
}
