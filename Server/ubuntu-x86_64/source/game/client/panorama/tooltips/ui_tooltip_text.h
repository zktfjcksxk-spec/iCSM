//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "ui_tooltip_base.h"

//-----------------------------------------------------------------------------
// Purpose: DOTA Tooltip for showing just text
//-----------------------------------------------------------------------------
class CUI_Tooltip_Text : public CUI_Tooltip_Base
{
	DECLARE_PANEL2D( CUI_Tooltip_Text, CUI_Tooltip_Base );

public:
	CUI_Tooltip_Text( panorama::CPanel2D *pParent, const char *pchName );
	CUI_Tooltip_Text( panorama::IUIWindow *pParent, const char *pchName );
	virtual ~CUI_Tooltip_Text();

	void SetText( const char *pszText );

private:
	void Initialize();

	panorama::CLabel *m_pTextLabel;
};