//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_custom_layout.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_CustomLayoutPanel, CustomLayoutPanel )

using namespace panorama;

CUI_CustomLayoutHandler::CUI_CustomLayoutHandler( panorama::CPanel2D *pTargetPanel )
	: m_pTargetPanel( pTargetPanel )
{
}

bool CUI_CustomLayoutHandler::BLoadCustomLayout( const char *pszLayoutFile, const char *pszParameters /* = nullptr */ )
{
	if ( !m_pTargetPanel.Get() )
	{
		AssertMsg( false, ( "Tried to set custom layout '%s' to a null panel.", pszLayoutFile ) );
		return false;
	}

	bool bLayoutChanged = m_strLayoutFile != pszLayoutFile;
	bool bParametersChanged = bLayoutChanged || ( pszParameters && ( m_strParameters != pszParameters ) );

	if ( !bLayoutChanged && !bParametersChanged )
		return true;

	m_strLayoutFile = pszLayoutFile;
	m_strParameters = pszParameters;

	if ( bParametersChanged )
	{
		// Clear existing parameters
		for ( const SParameter &parameter : m_vecParameters )
		{
			m_pTargetPanel->SetAttribute( parameter.strName, "" );
		}
		m_vecParameters.RemoveAll();
	}

	// Load the new layout if necessary
	if ( bLayoutChanged )
	{
		DbgVerify( m_pTargetPanel->BLoadLayout( pszLayoutFile, true ) );
	}

	// Set the new parameters
	if ( bParametersChanged )
	{
		if ( !V_isempty( pszParameters ) )
		{
			ParseUrlParameters( pszParameters, m_vecParameters );

			for ( const SParameter &parameter : m_vecParameters )
			{
				m_pTargetPanel->SetAttribute( parameter.strName, parameter.strValue );
			}
		}
	}

	return true;
}

// Takes a string of the form abc=123&def=213 and parse it into a vector
/*static*/ void CUI_CustomLayoutHandler::ParseUrlParameters( const char *pszParameters, CUtlVector< SParameter > &vecParameters )
{
	char szParamName[ 256 ];
	char szParamValue[ 256 ];
	size_t unLength = 0;

	const char *pszStart = pszParameters;
	while ( pszStart && pszStart[ 0 ] != '\0' )
	{
		const char *pszEquals = V_strstr( pszStart, "=" );
		const char *pszAmpersand = V_strstr( pszStart, "&" );

		szParamName[ 0 ] = '\0';
		szParamValue[ 0 ] = '\0';

		if ( !pszEquals && !pszAmpersand )
		{
			V_strcpy_safe( szParamName, pszStart );
			V_strcpy_safe( szParamValue, "1" );

			pszStart = nullptr;
		}
		else if ( pszEquals && !pszAmpersand )
		{
			unLength = Min( ( size_t )( pszEquals - pszStart + 1 ), sizeof( szParamName ) );
			if ( unLength > 0 )
				V_strncpy( szParamName, pszStart, unLength );

			V_strcpy_safe( szParamValue, pszEquals + 1 );

			pszStart = nullptr;
		}
		else if ( ( pszAmpersand && !pszEquals ) || ( pszAmpersand < pszEquals ) )
		{
			unLength = Min( ( size_t )( pszAmpersand - pszStart + 1 ), sizeof( szParamName ) );
			if ( unLength > 0 )
				V_strncpy( szParamName, pszStart, unLength );

			V_strcpy_safe( szParamValue, "1" );

			pszStart = pszAmpersand + 1;
		}
		else // pszAmpersand > pszEquals
		{
			unLength = Min( ( size_t )( pszEquals - pszStart + 1 ), sizeof( szParamName ) );
			if ( unLength > 0 )
				V_strncpy( szParamName, pszStart, unLength );

			unLength = Min( ( size_t )( pszAmpersand - pszEquals ), sizeof( szParamName ) );
			if ( unLength > 0 )
				V_strncpy( szParamValue, pszEquals + 1, unLength );

			pszStart = pszAmpersand + 1;
		}

		if ( szParamName[ 0 ] == '\0' )
			continue;

		// Might have URL encoded values
		Q_URLDecode( szParamName, sizeof( szParamName ), szParamName, sizeof( szParamName ) );
		Q_URLDecode( szParamValue, sizeof( szParamValue ), szParamValue, sizeof( szParamValue ) );

		vecParameters.AddToTail( { szParamName, szParamValue } );
	}
}

// ----------------------------------------------------------------------------

CUI_CustomLayoutPanel::CUI_CustomLayoutPanel( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_customLayoutHandler( this )
{
}

bool CUI_CustomLayoutPanel::BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symLayout( "layout" );

	if ( symName == k_symLayout )
	{
		// todo(ericl): can I do this here?
		return BLoadCustomLayout( pchValue );
	}

	return BaseClass::BSetProperty( symName, pchValue );
}