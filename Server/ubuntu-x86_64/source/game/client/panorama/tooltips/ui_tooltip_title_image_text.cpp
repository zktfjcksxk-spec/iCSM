//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_tooltip_title_image_text.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Tooltip_TitleImageText, TooltipTitleImageText )

using namespace panorama;

CUI_Tooltip_TitleImageText::CUI_Tooltip_TitleImageText( CPanel2D *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

CUI_Tooltip_TitleImageText::CUI_Tooltip_TitleImageText( IUIWindow *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

void CUI_Tooltip_TitleImageText::Initialize()
{
	CPanel2D *pContentsPanel = GetContentsPanel();
	DbgVerify( pContentsPanel->BLoadLayout( "file://{resources}/layout/tooltips/tooltip_title_image_text.xml" ) );
	m_pTitleLabel = assert_cast< CLabel * >( pContentsPanel->FindChildInLayoutFile( "TitleLabel" ) );
	m_pImagePanel = assert_cast< CImagePanel * >( pContentsPanel->FindChildInLayoutFile( "Image" ) );
	m_pTextLabel = assert_cast< CLabel * >( pContentsPanel->FindChildInLayoutFile( "TextLabel" ) );
}

CUI_Tooltip_TitleImageText::~CUI_Tooltip_TitleImageText()
{
}

void CUI_Tooltip_TitleImageText::SetParameters( const char *pszTitle, const char *pszImagePath, const char* pszText )
{
	SetTitle( pszTitle );
	SetImagePath( pszImagePath );
	SetText( pszText );
}

void CUI_Tooltip_TitleImageText::SetTitle( const char *pszTitle )
{
	m_pTitleLabel->SetTextWithDialogVariables( pszTitle );
}

void CUI_Tooltip_TitleImageText::SetImagePath( const char *pszImagePath )
{
	m_pImagePanel->SetImage( pszImagePath );
}

void CUI_Tooltip_TitleImageText::SetText( const char *pszText )
{
	m_pTextLabel->SetTextWithDialogVariables( pszText );
}