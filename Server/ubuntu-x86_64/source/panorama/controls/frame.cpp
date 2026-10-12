//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: <Frame src="file://{resources}/layout/example.xml" />
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/frame.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// Moved to panel2d.cpp to hack around the linker not including CFramePanel
// because client.dll has no references to CFramePanel.
//
// REGISTER_PANEL2D_FACTORY( CFramePanel, Frame );

CFramePanel::CFramePanel( CPanel2D* parent, const char* pchPanelID )
	: BaseClass( parent, pchPanelID )
	, m_bLayoutLoaded( false )
{
}

bool CFramePanel::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	const CPanoramaSymbol kSymSrc( "src" );
	const CPanoramaSymbol kSymSnippet( "snippet" );

	if ( symName == kSymSrc )
	{
		SetSource( pchValue );
		return true;
	}
	else if(symName == kSymSnippet)
	{
		SetSnippet( pchValue );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

void CFramePanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();
	RegisterJSMethod( "SetSource", PANORAMA_DELEGATE( &CFramePanel::SetSource ) );
	RegisterJSMethod( "SetSnippet", PANORAMA_DELEGATE( &CFramePanel::SetSnippet ) );
}

void CFramePanel::SetSource( const char* pchUrl )
{
	if ( m_bLayoutLoaded )
	{
		UnloadLayout();
		m_bLayoutLoaded = false;
	}

	if ( BLoadLayout( pchUrl, true ) )
	{
		m_bLayoutLoaded = true;
	}
}

void CFramePanel::SetSnippet( const char* pchSnippetName )
{
	if ( m_bLayoutLoaded )
	{
		UnloadLayout();
		m_bLayoutLoaded = false;
	}

	if ( BLoadLayoutSnippet( pchSnippetName ) )
	{
		m_bLayoutLoaded = true;
	}
}

