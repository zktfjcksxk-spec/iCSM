//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( VGUICENTERPRINT_H )
#define VGUICENTERPRINT_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/vgui.h>

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
namespace vgui
{
class Panel;
}

class CCenterStringLabel;
class CCenterPrint
{
private:
	CCenterStringLabel	*vguiCenterString;

public:
	enum EPriority
	{
		k_EPriority_None = 0, // Not shown
		k_EPriority_Low,
		k_EPriority_High,
	};
						CCenterPrint( void );

	virtual void		Create( vgui::VPANEL parent );
	virtual void		Destroy( void );
	
	virtual void		SetTextColor( int r, int g, int b, int a );
	virtual void		Print( const char *text, EPriority ePriority = k_EPriority_High );
	virtual void		Print( const wchar_t *text, EPriority ePriority = k_EPriority_High );
	virtual void		ColorPrint( int r, int g, int b, int a, const char *text );
	virtual void		ColorPrint( int r, int g, int b, int a, const wchar_t *text );
	virtual void		Clear( void );
	virtual void		PrintHint( const wchar_t* text );
};

extern CCenterPrint *GetCenterPrint();

#endif // VGUICENTERPRINT_H