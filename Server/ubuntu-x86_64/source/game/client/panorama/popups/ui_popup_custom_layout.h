//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "../ui_popup.h"
#include "panorama/ui_custom_layout.h"

class CUI_Popup_CustomLayout : public CUI_Popup
{
	DECLARE_PANEL2D( CUI_Popup_CustomLayout, CUI_Popup );

public:
	CUI_Popup_CustomLayout( panorama::CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent );

	void Init( const char *pszLayout, const char *pszParams );

private:
	CUI_CustomLayoutHandler m_customLayoutHandler;
};
