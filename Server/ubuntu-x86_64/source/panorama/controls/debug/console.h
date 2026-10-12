//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CONSOLEPANEL_H
#define CONSOLEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/iuiengine.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/label.h"
#include "panorama/uievent.h"
#include "panorama/controls/panelptr.h"

namespace panorama
{

DECLARE_PANORAMA_EVENT0( PostConsoleText );
DECLARE_PANORAMA_EVENT2( PostSpewText, SpewType_t, const char * );

class CConsoleInput;

//-----------------------------------------------------------------------------
// Purpose: Base panel that contains all other displayed debug panels
//-----------------------------------------------------------------------------
class CConsole : public CPanel2D
{
	DECLARE_PANEL2D( CConsole, CPanel2D );

public:
	CConsole( CTopLevelWindow *pWindow, const char *pchName );
	virtual ~CConsole();
	bool BSetProperty( CPanoramaSymbol symName, const char *pchValue );

	void AddText( const char *pchText );

	void ExecConsoleCommand( const char *pchCommand );
	bool OnTextEntrySubmit( const CPanelPtr< IUIPanel > &pPanel, const char *pchText );

	bool OnPostConsoleText();
	bool OnPostSpewText( SpewType_t spewType, const char *pMsg );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

private:
	CLabel *m_pConsoleText;
	CConsoleInput *m_pConsoleInput;
	int m_nMaxChars;
};

} // namespace panorama

#endif // CONSOLEPANEL_H