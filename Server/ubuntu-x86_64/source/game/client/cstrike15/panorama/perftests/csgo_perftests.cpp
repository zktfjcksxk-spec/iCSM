//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "panorama/csgo_panorama.h"
#include "csgo_perftests.h"

#include "csgo_perftests_item.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_PerfTests, CSGOPerfTestsCpp )


CCSGO_PerfTests::CCSGO_PerfTests( CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/tests/perf/perf_cpp.xml" );

	CPanel2D *pItemsParent = FindChildInLayoutFile("JsContent");
	for ( int i = 0; i < 200; ++i )
	{
		CCSGO_PerfTestsItem *pItem = new CCSGO_PerfTestsItem( pItemsParent, "" );
		pItem->SetID( i );
	}
}

class CCSGO_PanelTest1 : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_PanelTest1, panorama::CPanel2D );

public:
	CCSGO_PanelTest1( panorama::CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
	{
		RequireLoadLayout( "file://{resources}/layout/tests/perf/test_panel1.xml" );
		mX = 1;
	}

	int One()
	{
		Msg( "PanelTest1::One %d\n", mX );
		return mX;
	}

	virtual void SetupJavascriptObjectTemplate() OVERRIDE
	{
		BaseClass::SetupJavascriptObjectTemplate();

		RegisterJSMethod( "One", PANORAMA_DELEGATE( &CCSGO_PanelTest1::One ) );
	}

	int mX;
};
REGISTER_PANEL2D_FACTORY( CCSGO_PanelTest1, CSGOPanelTest1 )

class CCSGO_PanelTest2 : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_PanelTest2, panorama::CPanel2D );

public:
	CCSGO_PanelTest2( panorama::CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
	{
		RequireLoadLayout( "file://{resources}/layout/tests/perf/test_panel2.xml" );
		mY = 2;
	}

	int Two()
	{
		Msg( "PanelTest2::Two %d\n", mY );
		return mY;
	}

	virtual void SetupJavascriptObjectTemplate() OVERRIDE
	{
		BaseClass::SetupJavascriptObjectTemplate();

		RegisterJSMethod( "Two", PANORAMA_DELEGATE( &CCSGO_PanelTest2::Two ) );
	}

	int mY;
};
REGISTER_PANEL2D_FACTORY( CCSGO_PanelTest2, CSGOPanelTest2 )
