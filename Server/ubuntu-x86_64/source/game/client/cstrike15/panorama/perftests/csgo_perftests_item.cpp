//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_perftests_item.h"

#include "panorama/controls/label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_PerfTestsItem, CSGOPerfTestsCppItem )


CCSGO_PerfTestsItem::CCSGO_PerfTestsItem( CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/tests/perf/perf_item_cpp.xml" );

	m_pButtonLabel = panorama::panel_cast< panorama::CLabel * >( FindChildInLayoutFile( "buttonText" ) );
}

void CCSGO_PerfTestsItem::SetID( int nID )
{
	CUtlString labelString;
	labelString.Format( "Button_%03d", nID );
	m_pButtonLabel->SetText( labelString.Get() );
}