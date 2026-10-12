//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panelptr.h"
#include "panorama/panoramasymbol.h"
#include "panorama/controls/dropdown.h"

#include "csgo_iconvar_panorama_setter.h"

namespace panorama
{
	class CLabel;
	class CPanel2D;
};

class CCSGO_SettingsEnum : public panorama::CPanel2D, public CCSGO_iConvarPanoramaSetter
{
	DECLARE_PANEL2D( CCSGO_SettingsEnum, panorama::CPanel2D );

public:
	CCSGO_SettingsEnum( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SettingsEnum( );
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnShow();
	virtual ConVarRef& GetConVarRef( ) { return m_ConVar; }
	virtual void OnInitializedFromLayout() OVERRIDE;

private:
	bool EventButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );

	ConVarRef   m_ConVar;
	panorama::CLabel*		m_pLabel;
	panorama::CPanel2D*   m_pButtonPanel;
};

class CCSGO_SettingsEnumDropDown : public panorama::CDropDown, public CCSGO_iConvarPanoramaSetter
{
	DECLARE_PANEL2D( CCSGO_SettingsEnumDropDown, panorama::CDropDown );

public:

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	
	CCSGO_SettingsEnumDropDown( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SettingsEnumDropDown();

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnShow();
	virtual ConVarRef& GetConVarRef() OVERRIDE { return m_ConVar; }
	virtual void OnInitializedFromLayout() OVERRIDE;

	void RefreshDisplay(); 
	void RestoreCVarDefault();

private:
	bool EventDropdownSelectionChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	ConVarRef	m_ConVar;
};