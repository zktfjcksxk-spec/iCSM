//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

// Helper class to keep track of custom layout and parameters set on another panel
class CUI_CustomLayoutHandler
{
public:
	CUI_CustomLayoutHandler( panorama::CPanel2D *pTargetPanel );

	bool BLoadCustomLayout( const char *pszLayoutFile, const char *pszParameters = nullptr );

	struct SParameter
	{
		CUtlString strName;
		CUtlString strValue;
	};
	static void ParseUrlParameters( const char *pszParameters, CUtlVector< SParameter > &vecParameters );

private:
	panorama::CPanelPtr< panorama::CPanel2D > m_pTargetPanel;

	CUtlString m_strLayoutFile;
	CUtlString m_strParameters;

	CUtlVector< SParameter > m_vecParameters;
};


// Simple panel that lets you specify an XML attribute for layout="file://layout.xml" to load a layout
class CUI_CustomLayoutPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_CustomLayoutPanel, panorama::CPanel2D );

public:
	CUI_CustomLayoutPanel( panorama::CPanel2D *pParent, const char *pchID );

	bool BLoadCustomLayout( const char *pszLayoutFile, const char *pszParameters = nullptr ) { return m_customLayoutHandler.BLoadCustomLayout( pszLayoutFile, pszParameters ); }

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

private:
	CUI_CustomLayoutHandler m_customLayoutHandler;
};
