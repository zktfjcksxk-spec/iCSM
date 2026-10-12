//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

namespace panorama
{
	class CLabel;
}


DECLARE_PANEL_EVENT0( KeyBinderNewBind );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CCSGO_SettingsKeyBinder : public panorama::CPanel2D/*, public IInputHandler*/
{
	DECLARE_PANEL2D( CCSGO_SettingsKeyBinder, panorama::CPanel2D );

public:
	CCSGO_SettingsKeyBinder( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SettingsKeyBinder( );
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	void OnShow();
	virtual bool OnClick( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE;
	static bool HandleInputEvent( const InputEvent_t &inputEvent );
	static bool IsCapturingInput() { return sm_bIsAnyKeyBinderCapturingInput; }

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	void StartCapture();
	void ReleaseCapture();
	void Unbind();

	panorama::CLabel* m_pLabel;
	panorama::CLabel* m_pButton;
	panorama::CPanel2D* m_pLabelContainer;
	panorama::IUIPanel *m_pClearBindingPanel;
	panorama::IUIWindow* m_pParentWindow;

	const char *m_pszBindName;
	
	static ButtonCode_t sm_keyJustBound;
	static bool sm_bIsAnyKeyBinderCapturingInput;
	static CCSGO_SettingsKeyBinder *sm_pKeyBinderCapturingInput;
	static bool sm_bWasIMEAllowed;
	static bool s_bMouseOverClearButton;

	// Keep track of all active keybinders, so we can update them all when key binding changes
	static void UpdateAllActiveKeybinders();
	static CUtlVector< CCSGO_SettingsKeyBinder* > sm_vecActiveKeybinders; 
};
