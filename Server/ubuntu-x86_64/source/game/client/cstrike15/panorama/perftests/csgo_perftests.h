//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#ifndef CSGO_PERFTESTS_H
#define CSGO_PERFTESTS_H
#pragma once

#include "panorama/controls/panel2d.h"


//-----------------------------------------------------------------------------
// Purpose: CSGO Blur Test
// Test background blur...
//-----------------------------------------------------------------------------

class CCSGO_PerfTests : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_PerfTests, panorama::CPanel2D );

public:
	CCSGO_PerfTests( panorama::CPanel2D *pParent, const char *pchID );

};

#endif	// CSGO_PERFTESTS_H