//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "ui_tooltip_base.h"

//-----------------------------------------------------------------------------
// Purpose: Tooltip that loads an xml file for a custom layout
//-----------------------------------------------------------------------------
class CUI_Tooltip_CustomLayout : public CUI_Tooltip_Base
{
	DECLARE_PANEL2D( CUI_Tooltip_CustomLayout, CUI_Tooltip_Base );

public:
	CUI_Tooltip_CustomLayout( panorama::CPanel2D *pParent, const char *pchName );
	CUI_Tooltip_CustomLayout( panorama::IUIWindow *pParent, const char *pchName );
	virtual ~CUI_Tooltip_CustomLayout();

	void SetTooltipContents( const char *pszLayoutFile, const char *pszParameters );
	bool BContentsPanelLoaded( void ) const;

private:
	bool m_bContentsPanelLayoutLoaded;

};
