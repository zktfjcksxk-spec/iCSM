//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef DEBUGPANELPARENTS_H
#define DEBUGPANELPARENTS_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/iuiengine.h"
#include "panorama/input/iuiinput.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/panelptr.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Displays a horizontal list of the panel's parents
//-----------------------------------------------------------------------------
class CDebugPanelParents: public CPanel2D
{
	DECLARE_PANEL2D( CDebugPanelParents, CPanel2D );

public:
	CDebugPanelParents( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugPanelParents();

	void Build();	

private:
	// events
	bool EventSetDebugTarget( CPanelPtr< CPanel2D > pPanel );
	bool EventPanelStyleFlagsChanged( const CPanelPtr< IUIPanel > &pPanel );
	bool EventPanelStyleClassesChanged( const CPanelPtr< IUIPanel > &pPanel );
	
	CPanelPtr< CPanel2D > m_pDebugPanel;
	bool m_bRebuilding;
};

} // namespace panorama

#endif // DEBUGPANELPARENTS_H