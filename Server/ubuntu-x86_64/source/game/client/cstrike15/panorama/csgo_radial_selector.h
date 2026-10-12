//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Detects mouse position and dispatches events to child panels 
//
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/iuipanel.h"
#include "panorama/controls/panelptr.h"

DECLARE_PANEL_EVENT1( RadialSelectorHoverPanelChange, panorama::CPanelPtr< panorama::CPanel2D > );

class CCSGO_RadialSelector : public panorama::CPanel2D, public panorama::CDefaultInputCapture
{
	DECLARE_PANEL2D( CCSGO_RadialSelector, panorama::CPanel2D );

public:

	enum ELayoutType
	{
		k_eSixChoices,
		k_eFourChoices
	};

	CCSGO_RadialSelector( panorama::CPanel2D *pParent, const char *pchID, ELayoutType nLayoutType = k_eSixChoices );
	~CCSGO_RadialSelector();

	void Enable( void );
	void Disable( void );

	// CDefaultInputCapture
	virtual bool OnCapturedMouseMove( panorama::IUIPanel *pPanel, float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnCapturedMouseButtonUp( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE;

	panorama::CPanel2D* GetSelectedPanel( void ) const { return m_hSelectedPanel.Get(); }

protected:
	bool BSetProperties(const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties) OVERRIDE;

	void UpdateHoverPanel( CPanel2D *pPrev, CPanel2D *pCurSelection );
	bool EventMouseOver( const panorama::CPanelPtr< panorama::IUIPanel > & pPanel );
	bool EventMouseOut( const panorama::CPanelPtr< panorama::IUIPanel > & pPanel );

	float	m_angStart;
	panorama::CPanelPtr< panorama::CPanel2D > m_hSelectedPanel;

	CUtlString m_rolloverSound, m_clickSound;
};


