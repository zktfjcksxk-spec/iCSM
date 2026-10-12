//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Panorama menu for survival buymenu
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/uiinputcapture.h"
#include "cs_gamerules_survival.h"

DECLARE_PANORAMA_EVENT1( SurvivalBuyEvent, int );
DECLARE_PANORAMA_EVENT0( SurvivalBuyMenuUpdate );

class CCSGO_SurvivalBuyMenu : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_SurvivalBuyMenu, panorama::CPanel2D );

public:
	explicit CCSGO_SurvivalBuyMenu( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SurvivalBuyMenu();

	virtual void OnLayoutReloaded() OVERRIDE;

	bool ShowSurvivalBuyMenu( bool bShow );

	bool HandleBuyEvent( int nIndex );
	bool HandleUpdate( void );

protected:

	struct survivalBuyMenuItem
	{
		panorama::CPanelPtr<panorama::CPanel2D> m_pPanel;
		int nPrice;
	};
	CUtlVector<survivalBuyMenuItem> m_vecBuyMenuItems;
	void CreateBuyMenuItems( void );
	void DestroyBuyMenuItems( void );

	int m_nLastKnownPlayerAccount;
	EHANDLE m_hLastKnownPlayer;

	panorama::CPanelPtr< panorama::CPanel2D > m_pItemContainers[ TabletBuyMenuEntry::k_ECategoryCount ];

	panorama::CGameInputCapture m_Capture;
};
