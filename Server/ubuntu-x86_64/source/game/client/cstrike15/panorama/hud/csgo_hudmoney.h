//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_HUDMONEY_H_
#define CSGO_HUDMONEY_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?


class CCSGOMoneyAnimLabel;
class CCSGOMoneyPanelSymbols;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGOMoneyPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGOMoneyPanel, panorama::CPanel2D );

public:
	static const CCSGOMoneyPanelSymbols& Symbols();

	CCSGOMoneyPanel( panorama::CPanel2D *pParent, const char *pchID );

	void DoneAnimatingAdd();
	void DoneAnimatingSub();

	// Interface for users
	void SetPlayer( C_CSPlayer* pPlayer );
	void ShowPanel( bool bShow );
	void Reset();

private:
	void UpdateMoneyChange( int moneyDelta );
	void UpdateCurrentMoneyText();
	bool EventCSGOFrameUpdate();
	void Update();

	int m_nMoneyChange;

	int m_nLastMoney;
	CHandle<C_CSPlayer> m_hPlayer;

	bool m_bShowBuyZoneIcon;

	int m_nTotalAdd;
	int m_nTotalSub;

	CCSGOMoneyAnimLabel *m_pAddMoneyLabel;
	CCSGOMoneyAnimLabel *m_pAddMoneyFlashLabel;
	CCSGOMoneyAnimLabel *m_pRemoveMoneyLabel;
	panorama::CPanel2D *m_pMoneyBG;
};

class CCSGO_HudMoney : public CCSGOMoneyPanel, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudMoney, CCSGOMoneyPanel );

public:
	CCSGO_HudMoney( panorama::CPanel2D *pParent, const char *pchID );

	// CHudElement overrides
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGOMoneyAnimLabel : public panorama::CLabel
{
	DECLARE_PANEL2D( CCSGOMoneyAnimLabel, panorama::CLabel );

public:
	CCSGOMoneyAnimLabel( panorama::CPanel2D *pParent, const char *pchID );

private:
	static const CCSGOMoneyPanelSymbols& Symbols() { return CCSGOMoneyPanel::Symbols(); }

	panorama::CPanoramaSymbol m_CurrentAnimation;
	int m_nAnimationCount;
	bool EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, panorama::CPanoramaSymbol symAnimation );

	// Interface for CCSGO_HudMoney
	friend class CCSGOMoneyPanel;
	CCSGOMoneyPanel *m_pOwner;
	void StartAnimation( panorama::CPanoramaSymbol symClassName );
	void StopAnimation();
};

#endif	// CSGO_HUDHEALTHARMOR_H_