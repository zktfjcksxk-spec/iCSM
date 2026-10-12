//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for csgo main menu sales banner
//
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "gameui_interface.h"

class CCSGO_SalesBanner : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_SalesBanner, panorama::CPanel2D );
public:
	CCSGO_SalesBanner( panorama::CPanel2D *pParent, const char* pchID );
	virtual ~CCSGO_SalesBanner();


};