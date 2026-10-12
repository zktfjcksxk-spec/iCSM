//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef TOOLTIP_BASE_H
#define TOOLTIP_BASE_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/tooltip.h"
#include "panorama/ui_custom_layout.h"

#ifndef CSGO_PORT
// In Source1 the pointers_to_members pragma is used within Scaleform and VGUI headers to ensure member function pointers
// are maximum size. This affects the panorama javascript bindings (see uijsregistration.h).
// We also add the pragma here so that we always assume the larger size pointers in source 1 for consistency, regardless
// of the order of includes. 
#define MEMBER_FUNCPTRS_MAXSIZE
#pragma pointers_to_members( full_generality, virtual_inheritance )
#endif

namespace panorama
{
	class CTopLevelWindow;
}

class CUI_TooltipContents;

//-----------------------------------------------------------------------------
// Purpose: DOTA Tooltip for showing just text
//-----------------------------------------------------------------------------
class CUI_Tooltip_Base : public panorama::CTooltip
{
	DECLARE_PANEL2D( CUI_Tooltip_Base, panorama::CTooltip );

public:
	CUI_Tooltip_Base( panorama::CPanel2D *pParent, const char *pchName );
	CUI_Tooltip_Base( panorama::IUIWindow *pParent, const char *pchName );
	virtual ~CUI_Tooltip_Base();

	// This allows tooltips to get at the dialog variables of their associated panels
	void SetLocalizationParent( const panorama::CPanelPtr< panorama::IUIPanel >& localizationParent )
	{
		m_localizationParent = localizationParent;
	}

	virtual panorama::IUIPanel *GetLocalizationParent() const OVERRIDE
	{
		panorama::IUIPanel *pParentOverride = m_localizationParent.Get();
		if ( pParentOverride )
			return pParentOverride;
		return BaseClass::GetLocalizationParent();
	}

protected:
	CUI_TooltipContents *GetContentsPanel() const { return m_pContentsPanel; }

private:
	void Initialize();

	bool EventShowTooltip( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventHideTooltip( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	CUI_TooltipContents *m_pContentsPanel;
	panorama::CPanelPtr< panorama::IUIPanel > m_localizationParent;
};


//-----------------------------------------------------------------------------
// Purpose: Helper class to contain the contents of a tooltip
//-----------------------------------------------------------------------------
class CUI_TooltipContents : public CUI_CustomLayoutPanel
{
	DECLARE_PANEL2D( CUI_TooltipContents, CUI_CustomLayoutPanel );

public:
	CUI_TooltipContents( panorama::CPanel2D *pParent, const char *pchName );

	virtual bool BIsClientPanelEvent( panorama::CPanoramaSymbol symProperty ) OVERRIDE;

	void SetTooltip( CUI_Tooltip_Base *pTooltip ) { m_pTooltip = pTooltip; }

	panorama::IUIPanel *GetTooltipTarget() const { return m_pTooltip ? m_pTooltip->GetTooltipTarget() : nullptr; }

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	void FireTooltipLoadedEvent();
	void FireTooltipShownEvent();
	void FireTooltipHiddenEvent();

private:
	CUI_Tooltip_Base *m_pTooltip;
};

#endif // TOOLTIP_BASE_H
