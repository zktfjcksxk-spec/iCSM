//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_context_menu_custom_layout.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D( CUI_ContextMenu_CustomLayout, ContextMenuCustomLayout )


//-----------------------------------------------------------------------------
CUI_ContextMenu_CustomLayout::CUI_ContextMenu_CustomLayout( panorama::CPanel2D *pParent, const char *pchName, panorama::CPanel2D *pEventParent )
:
	CUI_ContextMenu_Base( pParent, pchName, pEventParent ),
	m_customLayoutHandler( GetContentsPanel() )
{
}

//-----------------------------------------------------------------------------
CUI_ContextMenu_CustomLayout::~CUI_ContextMenu_CustomLayout()
{
}

//-----------------------------------------------------------------------------
void CUI_ContextMenu_CustomLayout::Init( const char *pszLayout, const char *pszParams )
{
	DbgVerify( m_customLayoutHandler.BLoadCustomLayout( pszLayout, pszParams ) );
}