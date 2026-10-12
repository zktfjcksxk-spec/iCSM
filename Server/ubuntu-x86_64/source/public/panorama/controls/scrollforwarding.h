//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Panels for forwarding scroll input to other panels
//
//=============================================================================//

#ifndef SCROLLFORWARDING_H
#define SCROLLFORWARDING_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Forwards vertical scrolling input to another panel
//-----------------------------------------------------------------------------
class CVerticalScrollForwardingPanel : public CPanel2D
{
	DECLARE_PANEL2D( CVerticalScrollForwardingPanel, CPanel2D );

public:
	CVerticalScrollForwardingPanel( CPanel2D *pParent, const char * pchPanelID );	

	bool OnGamePadAnalog( const GamePadData_t &code ) OVERRIDE;
	bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
	bool OnMouseWheel( const MouseData_t &code ) OVERRIDE;

protected:
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

private:
	CPanel2D *GetTarget();

	CUtlString m_strTargetID;
	CPanelPtr< CPanel2D > m_ptrTarget;
};

} // namespace panorama

#endif // SCROLLFORWARDING_H