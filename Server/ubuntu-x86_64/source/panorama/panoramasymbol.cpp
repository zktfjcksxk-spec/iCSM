//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//


#include "stdafx.h"
#include "panorama/panoramasymbol.h"
#include "panorama/iuiengine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// Note:  Symbols ok to use at global construction time without a UIEngine inside the framework... 
// not ok outside in panel/client code though!
#ifdef PANORAMA_EXPORTS
namespace panorama
{
	// CPanoramaSymbol support for cross DLL symbols
	extern UtlSymId_t MakeSymbol( const char *pchText );

	// CPanoramaSymbol support for cross DLL symbols
	extern const char * ResolveSymbol( const UtlSymId_t sym );
}
#endif

CPanoramaSymbol::CPanoramaSymbol( char const* pStr )
{ 
#ifdef _DEBUG
	AssertMsg1( !pStr || !*pStr || !V_isspace( pStr[ V_strlen( pStr ) - 1 ] ), "CPanoramaSymbol has trailing whitespace: '%s'", pStr );
#endif

#ifdef PANORAMA_EXPORTS
	m_Id = panorama::MakeSymbol( pStr ); 
#else
	AssertMsg( UIEngine(), "Can't use CPanoramaSymbol outside panorama.dll until UIEngine is initialized!" );

	m_Id = UIEngine()->MakeSymbol( pStr );
#endif
}

// Gets the string associated with the symbol
char const* CPanoramaSymbol::String() const
{ 
#ifdef PANORAMA_EXPORTS
	return panorama::ResolveSymbol( m_Id ); 
#else
	AssertMsg( UIEngine(), "Can't use CPanoramaSymbol outside panorama.dll until UIEngine is initialized!" );
	return UIEngine()->ResolveSymbol( m_Id ); 
#endif
}

CUtlVector< CGlobalPanoramaSymbol::SPendingSymbol > *CGlobalPanoramaSymbol::s_pvecPendingSymbols = nullptr;

CGlobalPanoramaSymbol::CGlobalPanoramaSymbol( const char *pszValue )
{
	if ( panorama::UIEngine() )
	{
		m_sym = pszValue;
	}
	else
	{
		if ( s_pvecPendingSymbols == nullptr )
			s_pvecPendingSymbols = new CUtlVector< SPendingSymbol >();

		s_pvecPendingSymbols->AddToTail( { this, pszValue } );
	}
}

const panorama::CPanoramaSymbol &CGlobalPanoramaSymbol::GetSymbol() const
{
	Assert( panorama::UIEngine() );

	if ( !m_sym.IsValid() && panorama::UIEngine() && s_pvecPendingSymbols )
	{
		for ( SPendingSymbol &pending : *s_pvecPendingSymbols )
		{
			pending.pSymbol->m_sym = pending.pszValue;
		}
		
		delete s_pvecPendingSymbols;
		s_pvecPendingSymbols = nullptr;
	}

	return m_sym;
}