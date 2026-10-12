//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TOOLTIP_H
#define PANORAMA_TOOLTIP_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "../uievent.h"
#include "label.h"

namespace panorama
{

DECLARE_PANEL_EVENT0( TooltipVisible );
DECLARE_PANEL_EVENT0( TooltipHidden );

const char k_szHasTooltipTargetPosition[] = "has-tooltip-target-position";
const char k_szTooltipTargetX[] = "tooltip-target-x";
const char k_szTooltipTargetY[] = "tooltip-target-y";
const char k_szTooltipTargetWidth[] = "tooltip-target-width";
const char k_szTooltipTargetHeight[] = "tooltip-target-height";

//-----------------------------------------------------------------------------
// Purpose: Top level panel for a tooltip
//-----------------------------------------------------------------------------
class CTooltip : public CPanel2D
{
	DECLARE_PANEL2D( CTooltip, CPanel2D );

public:
	CTooltip( IUIWindow *pParent, const char *pchName );
	CTooltip( CPanel2D *pParent, const char *pchName );
	virtual ~CTooltip();

	void SetTooltipTarget( const CPanelPtr< IUIPanel >& targetPanelPtr );
	IUIPanel *GetTooltipTarget() const { return m_pTooltipTarget.Get(); }

	// Get/set tooltip visibility.  This is actually determined by a .css class,
	// so that transitions can be supported
	bool IsTooltipVisible() const;
	void SetTooltipVisible( bool bVisible );

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// request the tooltip to do positioning on the next layout
	virtual void CalculatePosition() { m_bReposition = true; m_ePrevPosition = k_EContextUIPositionUnset;  InvalidateSizeAndPosition(); }

private:

	void Init();
	void UpdatePosition();

	bool EventBaseTooltipVisible( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	bool m_bReposition;

	// Previous frame tooltip's position
	EContextUIPosition m_ePrevPosition;

	CPanelPtr< IUIPanel > m_pTooltipTarget;
};


//-----------------------------------------------------------------------------
// Purpose: Simple tooltip that just shows a string of text
//-----------------------------------------------------------------------------
class CTextTooltip : public CTooltip
{
	DECLARE_PANEL2D( CTextTooltip, CTooltip );

public:
	CTextTooltip( IUIWindow *pParent, const char *pchName, bool bInit = true  );
	CTextTooltip( CPanel2D *pParent, const char *pchName,  bool bInit = true  );
	virtual ~CTextTooltip();

	void SetText( const char *pchText, CLabel::ETextType eTextType = CLabel::k_ETextTypePlain );

	// clone
	virtual bool IsClonable() { return AreChildrenClonable(); }
	virtual CPanel2D *Clone();

protected:
	virtual void InitClonedPanel( CPanel2D *pPanel );

private:
	void Init();

	CLabel *m_pText;
};

} // namespace panorama

#endif // PANORAMA_TOOLTIP_H