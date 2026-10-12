//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "panorama/ui_js_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//
//	CUI_JS_Panel Methods
//
//-----------------------------------------------------------------------------

REGISTER_PANEL2D( CUI_JS_Panel, JSPanel );


//-----------------------------------------------------------------------------
CUI_JS_Panel::CUI_JS_Panel( panorama::CPanel2D *pParent, const char *pchID, panorama::CPanoramaSymbol jsPanelSymbol, const char *pszLayoutFile )
:
	panorama::CPanel2D( pParent, pchID ),
	m_jsPanelSymbol( jsPanelSymbol ),
	m_pUserProps( nullptr )
{
	RequireLoadLayout( pszLayoutFile );
}

//-----------------------------------------------------------------------------
CUI_JS_Panel::~CUI_JS_Panel()
{
	if ( m_pUserProps )
	{
		m_pUserProps->Reset();
		delete m_pUserProps;
		m_pUserProps = nullptr;
	}
}

//-----------------------------------------------------------------------------
panorama::CPanoramaSymbol CUI_JS_Panel::GetPanelType() const
{
	return m_jsPanelSymbol;
}
