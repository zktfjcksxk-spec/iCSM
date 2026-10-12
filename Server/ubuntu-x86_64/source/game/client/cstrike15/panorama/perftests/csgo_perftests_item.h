//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#ifndef CSGO_PERFTESTS_ITEM_H
#define CSGO_PERFTESTS_ITEM_H
#pragma once

#include "panorama/controls/panel2d.h"

namespace panorama
{
	class CLabel;
}

//-----------------------------------------------------------------------------
// Purpose: CSGO Blur Test
// Test background blur...
//-----------------------------------------------------------------------------

class CCSGO_PerfTestsItem : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_PerfTestsItem, panorama::CPanel2D );

public:

	CCSGO_PerfTestsItem( panorama::CPanel2D *pParent, const char *pchID );

	void SetID( int nID );

private:

	panorama::CLabel *m_pButtonLabel;
};

#endif	// CSGO_PERFTESTS_ITEM_H