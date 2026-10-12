//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/verticalscrolllist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CVerticalScrollList, VerticalScrollList );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVerticalScrollList::CVerticalScrollList( CPanel2D *parent, const char * pchPanelID ) : CPanel2D( parent, pchPanelID )
{

}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVerticalScrollList::~CVerticalScrollList() 
{

}


//-----------------------------------------------------------------------------
// Purpose: Set panel properties
//-----------------------------------------------------------------------------
bool CVerticalScrollList::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symChildTabIndexStart( "childtabindexstart" );
	static CPanoramaSymbol symChildSelectionPosStart( "childselectionposstart" );
	static CPanoramaSymbol symCountEvent( "countevent" );
	static CPanoramaSymbol symFreeEvent( "freeevent" );
	static CPanoramaSymbol symFactoryEvent( "factoryevent" );

	// Handle symbols
	if ( symName == symChildTabIndexStart )
	{
		return true;
	}
	else if ( symName == symChildSelectionPosStart )
	{
		return true;
	}
	else if ( symName == symCountEvent )
	{
		return true;
	}
	else if ( symName == symFreeEvent )
	{
		return true;
	}
	else if ( symName == symFactoryEvent )
	{
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}