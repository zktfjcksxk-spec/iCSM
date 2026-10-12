//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UI_JS_PANEL_H
#define UI_JS_PANEL_H
#pragma once

#include "panorama/controls/panel2d.h"

//-----------------------------------------------------------------------------
// CUI_JS_Panel class
//		Base class for all panels registered from javascript
//		
// TODO: Consider moving this class inside panorama. As of now it is only 
//		an experiment
//-----------------------------------------------------------------------------
class CUI_JS_Panel : public panorama::CPanel2D
{
public:
	// Not using DECLARE_PANEL2D macro on purpose

	typedef panorama::CPanel2D BaseClass;										
	typedef CUI_JS_Panel ThisClass;
	enum { kHasParentClass = 1 };
	static panorama::CPanoramaSymbol m_symbol;
	static const panorama::CPanel2DClassInfo m_PanelClassInfo;

	// Static type - will always return "JSPanel" (and not the name of type defined
	// when registering the panel in js)
	static panorama::CPanoramaSymbol GetPanelSymbol() { return ThisClass::m_symbol; }

	// Returns the name type name as defined in js when registering the panel 
	virtual panorama::CPanoramaSymbol GetPanelType() const OVERRIDE;

	// These always return the static type (JSPanel) as they are used for C++ casting
	static const panorama::CPanel2DClassInfo& GetPanelTypeClassInfo() { return ThisClass::m_PanelClassInfo; }
	virtual const panorama::CPanel2DClassInfo& GetPanelClassInfo() const OVERRIDE { return ThisClass::m_PanelClassInfo; }

public:

	CUI_JS_Panel( panorama::CPanel2D *pParent, const char *pchID, panorama::CPanoramaSymbol jsPanelSymbol, const char *pszLayoutFile );
	virtual ~CUI_JS_Panel();

private:

	panorama::CPanoramaSymbol m_jsPanelSymbol;

	v8::Persistent< v8::Object > *m_pUserProps;
};

#endif	// UI_JS_PANEL_H