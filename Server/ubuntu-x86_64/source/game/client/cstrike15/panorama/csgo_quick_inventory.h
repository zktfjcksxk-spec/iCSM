//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/uiinputcapture.h"

struct quickInvSegment
{
	panorama::CPanelPtr<panorama::CPanel2D> m_pPanel;
	float m_flAngleStart;
	float m_flAngleClockwiseWidth;
	CHandle<C_BaseCombatWeapon> m_hAssociatedWeapon;
};

class CCSGO_QuickInventory : public panorama::CPanel2D, public CPanoramaHudElement, public panorama::CDefaultInputCapture
{
	DECLARE_PANEL2D( CCSGO_QuickInventory, panorama::CPanel2D );

public:
	explicit CCSGO_QuickInventory( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_QuickInventory();

	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void Think( void );

	void DestroySegments( void );
	bool UpdateQuickInventoryRadial( void );

	virtual bool OnCapturedMouseMove( panorama::IUIPanel *pPanel, float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnCapturedMouseButtonUp( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE;
	
protected:
	
	CUtlVector<quickInvSegment> m_vecQuickInvSegments;

	const quickInvSegment *GetQuickInvSegmentFromMouseCoord( float flMouseX, float flMouseY );

	bool BSetProperties( const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties ) OVERRIDE;
	void UpdateHoverPanel( CPanel2D *pPrev, CPanel2D *pCurSelection );
	CUtlString m_rolloverSound, m_clickSound;

	panorama::CGameInputCapture m_Capture;
	panorama::CPanelPtr< panorama::CPanel2D > m_hSelectedPanel;

	void QuickSelectWeapon( C_BaseCombatWeapon *pWeapon );

	int HashLocalPlayerWeapons( void );
	int m_nLastKnownInvHash;
	bool m_bDoLastInv;
};