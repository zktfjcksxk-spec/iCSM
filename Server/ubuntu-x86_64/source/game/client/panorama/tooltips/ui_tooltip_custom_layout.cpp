//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_tooltip_custom_layout.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Tooltip_CustomLayout, TooltipCustomLayout )

using namespace panorama;

CUI_Tooltip_CustomLayout::CUI_Tooltip_CustomLayout( CPanel2D *pParent, const char *pchName ) 
	: CUI_Tooltip_Base( pParent, pchName ),
	m_bContentsPanelLayoutLoaded( false )
{
}

CUI_Tooltip_CustomLayout::CUI_Tooltip_CustomLayout( IUIWindow *pParent, const char *pchName )
	: CUI_Tooltip_Base( pParent, pchName ),
	m_bContentsPanelLayoutLoaded( false )
{
}

CUI_Tooltip_CustomLayout::~CUI_Tooltip_CustomLayout()
{
}

void CUI_Tooltip_CustomLayout::SetTooltipContents( const char *pszLayoutFile, const char *pszParameters )
{
	CUI_TooltipContents *pContentsPanel = GetContentsPanel();

	DbgVerify( pContentsPanel->BLoadCustomLayout( pszLayoutFile, pszParameters ) );
	m_bContentsPanelLayoutLoaded = true;

	pContentsPanel->FireTooltipLoadedEvent();
}

bool CUI_Tooltip_CustomLayout::BContentsPanelLoaded( void ) const
{
	return m_bContentsPanelLayoutLoaded;
}
