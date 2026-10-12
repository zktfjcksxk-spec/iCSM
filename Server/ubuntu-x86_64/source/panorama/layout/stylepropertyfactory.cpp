//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "stylepropertyfactory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: map of registered style property factory methods
//-----------------------------------------------------------------------------

struct StylePropertyFactoryRegistration_t
{
	char rgchStylePropertyString[256];
	CStylePropertyFactory * pFactory;
};

class CDefCaselessCUtlStringLess
{
public:
	CDefCaselessCUtlStringLess() {}
	CDefCaselessCUtlStringLess( int i ) {}
	inline bool operator()( const CUtlString &lhs, const CUtlString &rhs ) const { return ( V_stricmp(lhs.String(), rhs.String()) < 0 ); }
	inline bool operator!() const { return false; }
};

static int g_iStylePropertyIndexNextFree = 0;
static StylePropertyFactoryRegistration_t g_StylePropertyRegistrations[MAX_PANORAMA_STYLE_SYMBOLS];

// Wrap the accesses to the symbol map to solve initialization ordering issues
typedef CUtlMap< CUtlString, short, int, CDefCaselessCUtlStringLess > StyleSymbolMap_t;
static StyleSymbolMap_t &GetStyleSymbolMap()
{
	static StyleSymbolMap_t s_MapStyleSymbolStringsToIndex;
	return s_MapStyleSymbolStringsToIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CStyleSymbol::CStyleSymbol( char const* pStr ) 
{ 
	short i = GetStyleSymbolMap().Find( pStr );
	if ( i != GetStyleSymbolMap().InvalidIndex() )
	{
		m_Id = i;
	}
	else
	{
		m_Id = STYLE_SYMBOL_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CStyleSymbol::CStyleSymbol( char const* pStr, bool bCreateNew )
{
	short i = GetStyleSymbolMap().Find( pStr );
	if ( i != GetStyleSymbolMap().InvalidIndex() )
	{
		m_Id = i;
	}
	else 
	{
		if ( bCreateNew )
		{
			m_Id = g_iStylePropertyIndexNextFree++;

			AssertFatalMsg( g_iStylePropertyIndexNextFree < 255, "CStyleSymbol must become larger, cannot fit all style properties in uint8 anymore" );

			if( m_Id >= V_ARRAYSIZE( g_StylePropertyRegistrations ) )
				AssertFatalMsg1( false, "Need to increase size of static g_StylePropertyRegistrations (MAX_PANORAMA_STYLE_SYMBOLS) before registering more styles, failed on %s", pStr );

			GetStyleSymbolMap().Insert( pStr, m_Id );
			V_strcpy_safe( g_StylePropertyRegistrations[m_Id].rgchStylePropertyString, pStr );
		}
		else
		{
			m_Id = 0xFF;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: operator==
//-----------------------------------------------------------------------------
bool CStyleSymbol::operator==( char const* pStr ) const
{
	short i = GetStyleSymbolMap().Find( pStr );
	if ( i == m_Id )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Gets the string associated with the symbol
//-----------------------------------------------------------------------------
char const* CStyleSymbol::String() const
{
	if ( m_Id != 0xFF )
		return g_StylePropertyRegistrations[m_Id].rgchStylePropertyString;
	else
		return "";
}


//-----------------------------------------------------------------------------
// Purpose: style property factory static members
//-----------------------------------------------------------------------------
CUtlVector< CStyleSymbol > CStylePropertyFactory::s_vecAllProperties;
CUtlVector< CStyleSymbol > CStylePropertyFactory::s_vecInheritedProperties;
CUtlVector< CStyleSymbol > CStylePropertyFactory::s_vecPropertiesAndAliases;
CUtlVector< CUtlString > CStylePropertyFactory::s_vecSortedPropertyAndAliasNames;

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CStylePropertyFactory::CStylePropertyFactory( const char *pchName, STYLEPROPERTYCREATEFUNC pCreate, STYLEPROPERTYCOPYFUNC pCopy, STYLEPROPERTYFREEFUNC pFree, STYLEPROPERTYVALIDATEFUNC pValidate, STYLEPROPERTYMEMSTATSFUNC pMemStats, bool bCanInherit )
{
	CStyleSymbol symName( pchName );
	if ( BRegisteredPropertyOrAlias( symName ) )
		Plat_FatalError( "CStylePropertyFactory:  Factory for '%s' already exists!!!!\n", pchName );

	m_symbol = symName;
	m_funcCreate = pCreate;
	m_funcCopy = pCopy;
	m_funcFree = pFree;
	m_funcValidate = pValidate;
	m_funcMemStats = pMemStats;
	m_bAlias = false;
	m_bCanInherit = bCanInherit;

	g_StylePropertyRegistrations[ symName.GetID() ].pFactory = this;
}


//-----------------------------------------------------------------------------
// Purpose: constructor used when defining an alias
//-----------------------------------------------------------------------------
CStylePropertyFactory::CStylePropertyFactory( CStyleSymbol symProperty, STYLEPROPERTYCREATEFUNC pCreate, STYLEPROPERTYCOPYFUNC pCopy, STYLEPROPERTYFREEFUNC pFree, STYLEPROPERTYVALIDATEFUNC pValidate, STYLEPROPERTYMEMSTATSFUNC pMemStats, const char *pchAlias )
{
	CStyleSymbol symAlias( pchAlias );
	if ( BRegisteredPropertyOrAlias( symAlias ) )
		Plat_FatalError( "CStylePropertyFactory:  Factory for '%s' already exists!!!!\n", pchAlias );
	
	// symbol & create function should point to the real property
	m_symbol = symProperty;
	m_funcCreate = pCreate;
	m_funcCopy = pCopy;
	m_funcFree = pFree;
	m_funcValidate =  pValidate;
	m_funcMemStats = pMemStats;
	m_bAlias = true;
	m_bCanInherit = false;	// not set for property alias

	g_StylePropertyRegistrations[ symAlias.GetID() ].pFactory = this;
}


//-----------------------------------------------------------------------------
// Purpose: Check if specified type has been registered
//-----------------------------------------------------------------------------
bool CStylePropertyFactory::BRegisteredPropertyOrAlias( CStyleSymbol symName )
{
	uint8 symIndex = symName.GetID();
	if ( symIndex == 0xFF )
		return false;
		
	return g_StylePropertyRegistrations[ symIndex ].pFactory != NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Check if specified type has been registered and is not an alias
//-----------------------------------------------------------------------------
bool CStylePropertyFactory::BRegisteredProperty( CStyleSymbol symName )
{
	uint8 symIndex = symName.GetID();
	if ( symIndex == 0xFF )
		return false;

	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ symIndex ].pFactory;
	return pFactory && !pFactory->m_bAlias;
}


//-----------------------------------------------------------------------------
// Purpose: Check if specified type has been registered and is an alias
//-----------------------------------------------------------------------------
bool CStylePropertyFactory::BRegisteredAlias( CStyleSymbol symName )
{
	uint8 symIndex = symName.GetID();
	if ( symIndex == 0xFF )
		return false;

	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ symIndex ].pFactory;
	return pFactory && pFactory->m_bAlias;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if a specific property value can be inherited
//-----------------------------------------------------------------------------
bool CStylePropertyFactory::BCanInheritProperty( CStyleSymbol symName )
{
	uint8 symIndex = symName.GetID();
	if ( symIndex == 0xFF )
		return false;

	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ symIndex ].pFactory;
	return pFactory && pFactory->m_bCanInherit;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the property symbol an alias points to. 
//			If not an alias, returns provided symbol if registered, else invalid
//-----------------------------------------------------------------------------
CStyleSymbol CStylePropertyFactory::GetPropertyNameForAlias( CStyleSymbol symName )
{
	uint8 symIndex = symName.GetID();
	if ( symIndex == 0xFF )
		return CStyleSymbol();

	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ symIndex ].pFactory;
	if ( !pFactory )
		return CStyleSymbol();
	else
		return pFactory->m_symbol;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a style property
//-----------------------------------------------------------------------------
CStyleProperty *CStylePropertyFactory::CreateStyleProperty( CStyleSymbol symName )
{
	// Too fine grained to really want to commit this node, but useful when debugging styles perf
	VPROF_BUDGET_DETAILED( "CreateStyleProperty",  VPROF_BUDGETGROUP_TENFOOT );

	if ( !BRegisteredPropertyOrAlias( symName ) )
		return NULL;

	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ symName.GetID() ].pFactory;
	if ( !pFactory )
		return NULL;

	return pFactory->CreateStylePropertyInternal();
}


//-----------------------------------------------------------------------------
// Purpose: Creates the actual property
//-----------------------------------------------------------------------------
CStyleProperty *CStylePropertyFactory::CreateStylePropertyInternal()
{
	if ( !m_funcCreate )
	{
		AssertMsg( false, "CStylePropertyFactory::CreateStylePropertyInternal called on panel w/o a creation function" );
		return NULL;
	}

	return assert_cast< CStyleProperty * >(m_funcCreate());
}


//-----------------------------------------------------------------------------
// Purpose: Copy a style property
//-----------------------------------------------------------------------------
CStyleProperty *CStylePropertyFactory::CopyStyleProperty( const CStyleProperty &property  )
{
	// Too fine grained to really want to commit this node, but useful when debugging styles perf
	VPROF_BUDGET_DETAILED( "CopyStyleProperty", VPROF_BUDGETGROUP_TENFOOT );

	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ property.GetPropertySymbol().GetID() ].pFactory;
	if ( !pFactory )
		return NULL;

	return pFactory->CopyStylePropertyInternal( property );
}


//-----------------------------------------------------------------------------
// Purpose: Copy the actual property
//-----------------------------------------------------------------------------
CStyleProperty *CStylePropertyFactory::CopyStylePropertyInternal( const CStyleProperty &property )
{
	if ( !m_funcCopy )
	{
		AssertMsg( false, "CStylePropertyFactory::CopyStylePropertyInternal called on property w/o a creation function" );
		return NULL;
	}

	return assert_cast<CStyleProperty *>( m_funcCopy( property ) );
}


//-----------------------------------------------------------------------------
// Purpose: Frees a style property
//-----------------------------------------------------------------------------
void CStylePropertyFactory::FreeStyleProperty( CStyleProperty *pProperty )
{
	// Too fine grained to really want to commit this node, but useful when debugging styles perf
	VPROF_BUDGET_DETAILED( "FreeStyleProperty",  VPROF_BUDGETGROUP_TENFOOT );
	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ pProperty->GetPropertySymbol().GetID() ].pFactory;
	if ( !pFactory )
		return;

	pFactory->FreeStylePropertyInternal( pProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Frees a style property
//-----------------------------------------------------------------------------
void CStylePropertyFactory::FreeStylePropertyInternal( CStyleProperty *pProperty )
{
	if ( !m_funcFree )
	{
		AssertMsg( false, "CStylePropertyFactory::FreeStylePropertyInternal called on panel w/o a free function" );
		return;
	}

	m_funcFree( pProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Validates a style property's memory pool
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CStylePropertyFactory::ValidateStyleProperty( CStyleSymbol symName, CValidator &validator )
{
	CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[ symName.GetID() ].pFactory;
	if ( !pFactory )
		return;

	return pFactory->ValidateMemPool( validator );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Validates a style property's memory pool
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CStylePropertyFactory::ValidateMemPool( CValidator &validator )
{
	m_funcValidate( validator );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Returns all registered property symbols
//-----------------------------------------------------------------------------
const CUtlVector< CStyleSymbol > &CStylePropertyFactory::GetAllProperties()
{
	if ( s_vecAllProperties.Count() == 0 )
	{
		for( int i=0; i < g_iStylePropertyIndexNextFree; ++i )
		{
			CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[i].pFactory;
			if ( !pFactory->m_bAlias )
				s_vecAllProperties.AddToTail( pFactory->m_symbol );
		}
	}

	return s_vecAllProperties;
}


//-----------------------------------------------------------------------------
// Purpose: Returns all registered inherited property symbols
//-----------------------------------------------------------------------------
const CUtlVector< CStyleSymbol > &CStylePropertyFactory::GetInheritedProperties()
{
	if ( s_vecInheritedProperties.Count() == 0 )
	{
		for( int i=0; i < g_iStylePropertyIndexNextFree; ++i )
		{
			CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[i].pFactory;
			if ( pFactory->m_bCanInherit )
				s_vecInheritedProperties.AddToTail( pFactory->m_symbol );
		}
	}

	return s_vecInheritedProperties;
}


//-----------------------------------------------------------------------------
// Purpose: Returns all registered inherited property symbols
//-----------------------------------------------------------------------------
const CUtlVector< CStyleSymbol > &CStylePropertyFactory::GetPropertiesAndAliases()
{	
	if ( s_vecPropertiesAndAliases.Count() == 0 )
	{
		s_vecPropertiesAndAliases.EnsureCapacity( g_iStylePropertyIndexNextFree );
		for ( int i=0; i < g_iStylePropertyIndexNextFree; ++i )
		{
			s_vecPropertiesAndAliases.AddToTail( CStyleSymbol( i ) );
		}
	}

	return s_vecPropertiesAndAliases;
}


//-----------------------------------------------------------------------------
// Purpose: Returns an alphabetically sorted list of properties
//-----------------------------------------------------------------------------
const CUtlVector< CUtlString > &CStylePropertyFactory::GetSortedPropertyAndAliasNames()
{	
	if ( s_vecSortedPropertyAndAliasNames.Count() == 0 )
	{
		const CUtlVector< CStyleSymbol > &vecProperties = CStylePropertyFactory::GetPropertiesAndAliases();
		s_vecSortedPropertyAndAliasNames.EnsureCapacity( vecProperties.Count() );
		FOR_EACH_VEC( vecProperties, i )
		{
			s_vecSortedPropertyAndAliasNames.AddToTail( vecProperties[ i ].String() );
		}
		s_vecSortedPropertyAndAliasNames.Sort( &CompareStylePropertyName );
	}

	return s_vecSortedPropertyAndAliasNames;
}

//-----------------------------------------------------------------------------
// Purpose: Prints out mem stats of all valid panorama CSS properties
//-----------------------------------------------------------------------------
void CStylePropertyFactory::PrintPropertiesMemStats()
{
	StylePropertyMemStats_t stats;
	StylePropertyMemStats_t totStats;
	memset( &totStats, 0, sizeof( totStats ) );

	DevMsg( "===== Mem stats of all css properties =====\n" );
	for ( int i = 0; i < g_iStylePropertyIndexNextFree; ++i )
	{
		CStylePropertyFactory *pFactory = g_StylePropertyRegistrations[i].pFactory;
		if ( !pFactory->m_bAlias )
		{
			pFactory->m_funcMemStats( stats );

			if ( stats.m_nPeakCount > 0 )
			{
				DevMsg( "%-22s - size: %6d bytes / count: %4d / peak size: %6d bytes / peak count: %4d\n", pFactory->m_symbol.String(), stats.m_nSizeInBytes, stats.m_nCount, stats.m_nPeakSizeInBytes, stats.m_nPeakCount );
				
				totStats.m_nSizeInBytes += stats.m_nSizeInBytes;
				totStats.m_nCount += stats.m_nCount;
				totStats.m_nPeakSizeInBytes += stats.m_nPeakSizeInBytes;
				totStats.m_nPeakCount += stats.m_nPeakCount;
			}
		}
	}
	DevMsg( "----------\nTotal size: %d bytes\nTotal count: %d\nTotal peak size: %d bytes\nTotal peak count: %d\n", totStats.m_nSizeInBytes, totStats.m_nCount, totStats.m_nPeakSizeInBytes, totStats.m_nPeakCount );
	DevMsg( "==========================================\n" );
}


#ifdef PANORAMA_EXPORTS // only in panorama.dll so that it doesn't run twice
//-----------------------------------------------------------------------------
// Purpose: Dumps the list of all valid panorama css properties and their documentation
//-----------------------------------------------------------------------------
CON_COMMAND_F( dump_panorama_css_properties, "Prints out all valid panorama CSS properties and their documentation", 0 )
{
	const CUtlVector< CUtlString > &vecProperties = UIEngine()->UIStyleFactory()->GetSortedPropertyAndAliasNames();

	for ( int i = 0; i < vecProperties.Count(); ++i )
	{
		CStyleSymbol symProperty( vecProperties[ i ].Get() );

		Msg( "=== %s ===\n", symProperty.String() );

		CStyleProperty *pStyleProperty = UIEngine()->UIStyleFactory()->CreateStyleProperty( symProperty );

		const char *pszDescription = pStyleProperty->GetDescription( symProperty );
		Msg( "%s\n\n\n", pszDescription );

		UIEngine()->UIStyleFactory()->FreeStyleProperty( pStyleProperty );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dumps mem stats valid panorama css properties
//-----------------------------------------------------------------------------
CON_COMMAND_F( dump_panorama_css_properties_memstats, "Prints out mem stats of all valid panorama CSS properties", 0 )
{
	CStylePropertyFactory::PrintPropertiesMemStats();
}
#endif


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate statics
//-----------------------------------------------------------------------------
void CStylePropertyFactory::ValidateStatics( CValidator &validator )
{
	// Ensure we have the list built by calling getter first..
	GetAllProperties();
	FOR_EACH_VEC( s_vecAllProperties, i )
	{
		CStylePropertyFactory::ValidateStyleProperty( s_vecAllProperties[i], validator );
	}

	ValidateObj( s_vecAllProperties );
	ValidateObj( s_vecInheritedProperties );
	ValidateObj( s_vecPropertiesAndAliases );

	ValidateObj( s_vecSortedPropertyAndAliasNames );
	FOR_EACH_VEC( s_vecSortedPropertyAndAliasNames, i )
	{
		ValidateObj( s_vecSortedPropertyAndAliasNames[i] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: validate the global map
//-----------------------------------------------------------------------------
void panorama::ValidateStylePropertyFactory( CValidator &validator )
{
	ValidateObj( GetStyleSymbolMap() );
	FOR_EACH_MAP_FAST( GetStyleSymbolMap(), i )
	{
		ValidateObj( GetStyleSymbolMap().Key( i ) );
	}
	CStylePropertyFactory::ValidateStatics( validator );	
}

#endif

