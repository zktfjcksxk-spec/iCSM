//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TABLETBUYMENU_H
#define TABLETBUYMENU_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
//#include "cs_shareddefs.h"

class C_CSPlayer;

class CTabletBuymenu : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CTabletBuymenu, vgui::Frame );

public:
	CTabletBuymenu(vgui::Panel *parent);
	~CTabletBuymenu();

	virtual void OnClose( void );
	virtual void OnCommand( const char *command );
	virtual void OnMessage( const KeyValues *pParams,  vgui::VPANEL fromPanel );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	C_CSPlayer *GetLocalTabletCSPlayer( void );

	//bool ResolveLocalPlayerHoldingTablet( void );

};

#endif //TABLETBUYMENU_H