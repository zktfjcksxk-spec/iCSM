//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef C_WORLD_VGUI_TEXT_H
#define C_WORLD_VGUI_TEXT_H

#include "cbase.h"

class C_WorldVguiText : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_WorldVguiText, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_WorldVguiText();
	~C_WorldVguiText();

	bool IsEnabled( void ) const { return m_bEnabled; }
	
	virtual void ReceiveMessage( int classID, bf_read &msg );

	const char *GetDisplayText( void ) const { return m_szDisplayText; }
	const char *GetDisplayTextOption( void ) const { return m_szDisplayTextOption; }
	const char *GetFont( void ) const { return m_szFont; }
	int GetTextPanelWidth() { return m_iTextPanelWidth; }
	Color	GetColor() { return m_clrText; }

private:
	bool	m_bEnabled;
	char	m_szDisplayText[512];
	char	m_szDisplayTextOption[256];
	char	m_szFont[64];
	Color	m_clrText;
	int		m_iTextPanelWidth;
};

#endif //C_WORLD_VGUI_TEXT_H