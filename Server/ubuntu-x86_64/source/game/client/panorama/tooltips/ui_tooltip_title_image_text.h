//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "ui_tooltip_base.h"

//-----------------------------------------------------------------------------
// Purpose: DOTA Tooltip for showing just text
//-----------------------------------------------------------------------------
class CUI_Tooltip_TitleImageText : public CUI_Tooltip_Base
{
	DECLARE_PANEL2D( CUI_Tooltip_TitleImageText, panorama::CTooltip );

public:
	CUI_Tooltip_TitleImageText( panorama::CPanel2D *pParent, const char *pchName );
	CUI_Tooltip_TitleImageText( panorama::IUIWindow *pParent, const char *pchName );
	virtual ~CUI_Tooltip_TitleImageText();

	void SetParameters( const char *pszTitle, const char *pszImagePath, const char* pszText );

	void SetTitle( const char *pszTitle );
	void SetImagePath( const char *pszImagePath );
	void SetText( const char *pszText );

private:
	void Initialize();

	panorama::CLabel *m_pTitleLabel;
	panorama::CImagePanel *m_pImagePanel;
	panorama::CLabel *m_pTextLabel;
};

