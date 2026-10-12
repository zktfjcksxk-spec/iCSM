//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "ui_tooltip_base.h"

//-----------------------------------------------------------------------------
// Purpose: Tooltip for showing just text
//-----------------------------------------------------------------------------
class CUI_Tooltip_TitleText : public CUI_Tooltip_Base
{
	DECLARE_PANEL2D( CUI_Tooltip_TitleText, CUI_Tooltip_Base );

public:
	CUI_Tooltip_TitleText( panorama::CPanel2D *pParent, const char *pchName );
	CUI_Tooltip_TitleText( panorama::IUIWindow *pParent, const char *pchName );
	virtual ~CUI_Tooltip_TitleText();

	void SetParameters( const char *pszTitle, const char* pszText );

	void SetTitle( const char *pszTitle );
	void SetText( const char *pszText );

private:
	void Initialize();

	panorama::CLabel *m_pTitleLabel;
	panorama::CLabel *m_pTextLabel;
};
