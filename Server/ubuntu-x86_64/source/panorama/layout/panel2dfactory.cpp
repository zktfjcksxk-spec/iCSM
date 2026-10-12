//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//


#include "stdafx.h"
#include "panorama/layout/panel2dfactory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

struct LocalPanelRegistration_t
{
	CPanel2DFactory *pFactory;
	CPanoramaSymbol *pSym;
};

//-----------------------------------------------------------------------------
// Purpose: map of registered panel factory methods
//-----------------------------------------------------------------------------
typedef CUtlMap< CUtlSymbol, LocalPanelRegistration_t, int, CDefLess< CUtlSymbol > > MapPanel2DFactory_t;
MapPanel2DFactory_t &GetMapLocalPanel2DFactory()
{
	static MapPanel2DFactory_t s_mapPanel2DFactory;
	return s_mapPanel2DFactory;
}

void panorama::RegisterPanelFactoriesWithEngine( IUIEngine *pEngine )
{
	MapPanel2DFactory_t &mapClientPanels = GetMapLocalPanel2DFactory();
	FOR_EACH_MAP_FAST( mapClientPanels, i )
	{
		LocalPanelRegistration_t &reg = mapClientPanels.Element( i );
		*(reg.pSym) = mapClientPanels.Key( i ).String();
		pEngine->RegisterPanelFactoryWithEngine( *(reg.pSym), reg.pFactory );
	}
}


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPanel2DFactory::CPanel2DFactory( CPanoramaSymbol *pSym, char const *pchLayoutName, PANEL2DCREATEFUNC func, void *pChrisBoydBS, PANELSYMBOLFUNC symParentFunc, void *pUserData /*= NULL*/ )
{
	CUtlSymbol symLayoutName( pchLayoutName );
	if ( BRegisteredLocalName( symLayoutName ) )
		Plat_FatalError( "CPanel2DFactory:  Factory for '%s' already exists!!!!\n", pchLayoutName );

	m_funcCreate = func;
	m_pUserdata = pUserData;
	m_funcSymParent = symParentFunc;

	LocalPanelRegistration_t reg;
	reg.pFactory = this;
	reg.pSym = pSym;

	GetMapLocalPanel2DFactory().InsertOrReplace( symLayoutName, reg );
}


//-----------------------------------------------------------------------------
// Purpose: Check if specified type has been registered
//-----------------------------------------------------------------------------
bool CPanel2DFactory::BRegisteredLocalName( CUtlSymbol symName )
{
	return GetMapLocalPanel2DFactory().HasElement( symName );
}


//-----------------------------------------------------------------------------
// Purpose: Creates the actual panel
//-----------------------------------------------------------------------------
IUIPanelClient *CPanel2DFactory::CreatePanelInternal( const char *pchID, IUIPanel *parent )
{
	if ( !m_funcCreate )
	{
		AssertMsg( false, "CPanel2DFactory::CreatePanelInternal called on panel w/o a creation function" );
		return NULL;
	}

	return m_funcCreate( pchID, parent, m_pUserdata );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate the global map
//-----------------------------------------------------------------------------
void panorama::ValidatePanel2DFactory( CValidator &validator )
{
	ValidateObj( GetMapLocalPanel2DFactory() );
}
#endif


