//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: <Frame src="file://{resources}/layout/example.xml" />
//=============================================================================//

#ifndef PANORAMA_FRAMEPANEL_H
#define PANORAMA_FRAMEPANEL_H
#pragma once

#include "panel2d.h"

namespace panorama
{

	class CFramePanel : public CPanel2D
	{
		DECLARE_PANEL2D( CFramePanel, CPanel2D );

	public:
		CFramePanel( CPanel2D* parent, const char* pchPanelID );

		// CPanel2D overrides
		virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
		virtual void SetupJavascriptObjectTemplate() OVERRIDE;

		void SetSource( const char *pchUrl );
		void SetSnippet( const char* pchSnippetName );

	private:
		bool m_bLayoutLoaded;
	};

} // namespace panorama

#endif // PANORAMA_FRAMEPANEL_H