//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_tooltip_title_text.h"
#include "panorama/controls/label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Tooltip_TitleText, TooltipTitleText )

using namespace panorama;

CUI_Tooltip_TitleText::CUI_Tooltip_TitleText( CPanel2D *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

CUI_Tooltip_TitleText::CUI_Tooltip_TitleText( IUIWindow *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

void CUI_Tooltip_TitleText::Initialize()
{
	CPanel2D *pContentsPanel = GetContentsPanel();
	DbgVerify( pContentsPanel->BLoadLayout( "file://{resources}/layout/tooltips/tooltip_title_text.xml" ) );
	m_pTitleLabel = panel_cast< CLabel * >( pContentsPanel->FindChildInLayoutFile( "TitleLabel" ) );
	m_pTextLabel = panel_cast< CLabel * >( pContentsPanel->FindChildInLayoutFile( "TextLabel" ) );
}

CUI_Tooltip_TitleText::~CUI_Tooltip_TitleText()
{
}

void CUI_Tooltip_TitleText::SetParameters( const char *pszTitle, const char* pszText )
{
	SetTitle( pszTitle );
	SetText( pszText );
}

void CUI_Tooltip_TitleText::SetTitle( const char *pszTitle )
{
	m_pTitleLabel->SetTextWithDialogVariables( pszTitle );
}

void CUI_Tooltip_TitleText::SetText( const char *pszText )
{
	m_pTextLabel->SetTextWithDialogVariables( pszText );
}