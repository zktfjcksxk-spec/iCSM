//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_tooltip_text.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Tooltip_Text, TooltipText )

using namespace panorama;

CUI_Tooltip_Text::CUI_Tooltip_Text( CPanel2D *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

CUI_Tooltip_Text::CUI_Tooltip_Text( IUIWindow *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

void CUI_Tooltip_Text::Initialize()
{
	CPanel2D *pContentsPanel = GetContentsPanel();
	DbgVerify( pContentsPanel->BLoadLayout( "file://{resources}/layout/tooltips/tooltip_text.xml" ) );
	m_pTextLabel = panel_cast< CLabel * >( pContentsPanel->FindChildInLayoutFile( "TextLabel" ) );
}

CUI_Tooltip_Text::~CUI_Tooltip_Text()
{
}

void CUI_Tooltip_Text::SetText( const char *pszText )
{
	m_pTextLabel->SetTextWithDialogVariables( pszText );
}