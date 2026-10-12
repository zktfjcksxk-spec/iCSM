//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_FULLSCREEN_H
#define PANORAMA_TEXTINPUT_FULLSCREEN_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"
#include "panorama/controls/textentry.h"

namespace panorama
{

DECLARE_PANEL_EVENT2( TextInputFullscreenClosed, bool, const char * );

DECLARE_PANEL_EVENT0( TextInputFullscreenPositionChanged );

//-----------------------------------------------------------------------------
// Purpose: Full screen + daisy wheel
//-----------------------------------------------------------------------------
class CTextInputFullscreen : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CTextInputFullscreen, panorama::CPanel2D );

public:
	CTextInputFullscreen( panorama::CPanel2D *pPanel, const char * pchPanelID, const CTextInputHandlerSettings &settings );
	~CTextInputFullscreen();

	void SetMultiline( bool bMultiline );
	void SetDescription( const char *pchDescription );
	void SetMaxChars( uint32 unCharMax );
	void SetEnteredText( const char *pchText );
	void SetMinimalMode( bool bMinimal );
	virtual panorama::IUIPanel *OnGetDefaultInputFocus() OVERRIDE;
		
private:
	bool EventTextInputHandlerStateChange( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, bool bActivating );
	bool EventTextInputFinished( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, bool bSubmitted, const char *pchText );
	bool EventTextInputUnhandledButtonPress( panorama::GamePadData_t code );
	
	panorama::CTextInputHandler *m_pTextInputHandler;
	panorama::CTextEntry *m_pEnteredText;
	panorama::CLabel *m_pInputDescription;
	
	bool bDockedLeft;
	bool bDockedTop;
	
	uint unCurrentPosition;
	
	bool m_bMinimal;
};

} // namespace panorama

#endif // PANORAMA_TEXTINPUT_FULLSCREEN_H

