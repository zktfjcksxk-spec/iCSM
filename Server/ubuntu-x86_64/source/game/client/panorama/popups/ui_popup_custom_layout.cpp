//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_popup_custom_layout.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Popup_CustomLayout, PopupCustomLayout )

using namespace panorama;

CUI_Popup_CustomLayout::CUI_Popup_CustomLayout( CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent )
	: CUI_Popup( pParent, pchID, pEventParent )
	, m_customLayoutHandler( this )
{
}

void CUI_Popup_CustomLayout::Init( const char *pszLayout, const char *pszParams )
{
	DbgVerify( m_customLayoutHandler.BLoadCustomLayout( pszLayout, pszParams ) );
}