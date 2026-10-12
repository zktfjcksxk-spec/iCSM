//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#include "kv3format_manager.h"
#include "tier1/utlhashtable.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3FormatManager g_KV3FormatConverter;


//--------------------------------------------------------------------------------------------------
// Force kv3formats.cpp not to be elided by the linker
//--------------------------------------------------------------------------------------------------
extern int g_KV3Formats_LinkerHook;
static int s_pKV3Formats = g_KV3Formats_LinkerHook;
int g_KV3Format_Manager_LinkerHook = 0;


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool ConvertKV3Format( KeyValues3 *pRoot, const KV3ID_t &fromFormat, const KV3ID_t &toFormat, CUtlString *pOutError )
{
	return g_KV3FormatConverter.Convert( pRoot, fromFormat, toFormat, pOutError );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static void KV3IDToName( const KV3ID_t &id, CUtlString *pOutName )
{
	if ( id.m_pName && id.m_pName[0] != '\0' )
	{
		// use the friendly name if present
		*pOutName = id.m_pName;
	}
	else
	{
		// otherwise convert the raw UUID (eg. we loaded a binary file)
		CUUIDString str( id.m_UUID );
		*pOutName = str;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3FormatManager::CKV3FormatManager()
	: m_AllConversions()
	, m_bAllowNewRegistrations(true)
{
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3FormatManager::~CKV3FormatManager()
{
	m_AllConversions.PurgeAndDeleteElements();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3FormatManager::RegisterFormatConversion( const KV3ID_t &fromFormat, const KV3ID_t &toFormat, ConversionFn_t pConversionFn )
{
	if ( !m_bAllowNewRegistrations )
	{
		// for now we just firewall this - assume that we register all conversions before we ever perform one
		// (otherwise we'll have to handle thread safety and invalidate any cached paths)
		CUtlString fromName;
		CUtlString toName;
		KV3IDToName( fromFormat, &fromName );
		KV3IDToName( toFormat, &toName );
		Plat_FatalError( "Trying to register KV3 conversion too late (from '%s' to '%s')", fromName.Get(), toName.Get() );
	}

	if ( fromFormat == toFormat )
	{
		CUtlString fromName;
		CUtlString toName;
		KV3IDToName( fromFormat, &fromName );
		KV3IDToName( toFormat, &toName );
		Plat_FatalError( "Cannot register same format from/to a KV3 conversion (from '%s' to '%s')", fromName.Get(), toName.Get() );
	}

	for ( int iConversion = 0; iConversion < m_AllConversions.Count(); ++iConversion )
	{
		Conversion_t *pConversion = m_AllConversions[ iConversion ];
		if ( pConversion->m_FromFormat == fromFormat && pConversion->m_ToFormat == toFormat )
		{
			CUtlString fromName;
			CUtlString toName;
			KV3IDToName( fromFormat, &fromName );
			KV3IDToName( toFormat, &toName );
			Plat_FatalError( "Double-register of KV3 conversion (from '%s' to '%s')", fromName.Get(), toName.Get() );
		}
	}

	Conversion_t *pConversion = new Conversion_t( fromFormat, toFormat, pConversionFn );
	m_AllConversions.AddToTail( pConversion );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKV3FormatManager::FindConversionPath( ConversionPath_t *pOutPath, const KV3ID_t &fromFormat, const KV3ID_t &toFormat ) const
{
	// (TODO: cache result)
	return FindConversionPath_R( pOutPath, fromFormat, toFormat );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKV3FormatManager::FindConversionPath_R( ConversionPath_t *pOutPath, const KV3ID_t &fromFormat, const KV3ID_t &toFormat ) const
{
	if ( fromFormat == toFormat )
		return true;

	// brute force DFS - no attempt to detect multiple paths
	// we search from destination format backwards because there probably are fewer incoming edges than outgoing edges
	// (particularly when you consider the initial generic format that will connect to many others)
	for ( int iConversion = 0; iConversion < m_AllConversions.Count(); ++iConversion )
	{
		Conversion_t *pConversion = m_AllConversions[ iConversion ];
		if ( pConversion->m_ToFormat != toFormat )
		{
			continue;
		}

		if ( pConversion->m_bCurrentlyVisitingDuringSearch )
		{
			continue;
		}

		pConversion->m_bCurrentlyVisitingDuringSearch = true;

		if ( FindConversionPath_R( pOutPath, fromFormat, pConversion->m_FromFormat ) )
		{
			pOutPath->AddToTail( pConversion );
			pConversion->m_bCurrentlyVisitingDuringSearch = false;
			return true;
		}

		pConversion->m_bCurrentlyVisitingDuringSearch = false;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKV3FormatManager::Convert( KeyValues3 *pRoot, const KV3ID_t &fromFormat, const KV3ID_t &toFormat, CUtlString *pOutError ) const
{
	m_bAllowNewRegistrations = false; // lock out future registration so that we don't get confused

	//------------------------------------------------------------------------------
	// Find the conversion steps between the specified formats
	//------------------------------------------------------------------------------
	ConversionPath_t conversionPath;
	if ( !FindConversionPath( &conversionPath, fromFormat, toFormat ) )
	{
		 if ( pOutError )
		 {
			 CUtlString fromName;
			 CUtlString toName;
			 KV3IDToName( fromFormat, &fromName );
			 KV3IDToName( fromFormat, &fromName );

			 pOutError->Format( "No valid format conversion from '%s' to '%s'", fromName.Get(), toName.Get() );
		 }
		 return false;
	}

	//------------------------------------------------------------------------------
	// Execute the steps
	//------------------------------------------------------------------------------
	const KV3ID_t *pCurrentFormat = &fromFormat;

	for ( int iStep = 0; iStep < conversionPath.Count(); ++iStep )
	{
		 Conversion_t *pStep = conversionPath[ iStep ];
		 Assert( *pCurrentFormat == pStep->m_FromFormat );

		 KV3FormatConversionContext_t ctx;
		 ctx.m_pRoot = pRoot;

		 if ( pStep->m_pConversionFn( ctx ) != KV3_FORMAT_CONVERSION_SUCCESS )
		 {
			 if ( pOutError )
			 {
				*pOutError = ctx.m_OutError;
			 }
			 return false;
		 }

		 pCurrentFormat = &pStep->m_ToFormat;
	}

	Assert( *pCurrentFormat == toFormat );
	return true;
}
