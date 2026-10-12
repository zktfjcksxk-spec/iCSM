//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for csgo main menu sales banner
//
//=============================================================================//

#include "cbase.h"
#include "csgo_store.h"

REGISTER_PANEL2D_FACTORY( CCSGO_SalesBanner, SalesBanner );

CCSGO_SalesBanner* g_pCSGOSalesBanner = NULL;

using namespace panorama;

CCSGO_SalesBanner::CCSGO_SalesBanner( CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
	Assert( !g_pCSGOSalesBanner );
	g_pCSGOSalesBanner = this;

	DbgVerify( BLoadLayout( "file://{resources}/layout/store.xml" ) );
	SetAcceptsInput( true );
}

CCSGO_SalesBanner::~CCSGO_SalesBanner()
{
}

