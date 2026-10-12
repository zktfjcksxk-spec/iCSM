//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_page.h"
#include "ui_page_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

CUI_Page::CUI_Page( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_unPageNavigateID( 0 )
{
	SetTabIndex( k_flTabIndexAuto );
	SetAcceptsFocus( true );
}

const char *CUI_Page::GetPrimaryTab()
{
	return NULL;
}

bool CUI_Page::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	static const CPanoramaSymbol k_symOnPageSetupSuccess( "onpagesetupsuccess" );
	if ( symProperty == k_symOnPageSetupSuccess )
		return true;

	return BaseClass::BIsClientPanelEvent( symProperty );
}

bool CUI_Page::IsPageActive() const
{
	return GetPageNavigateID() == CUI_PageManager::GetInstance()->GetCurrentPageNavigateID();
}
