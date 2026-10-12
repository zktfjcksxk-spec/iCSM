//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/ui_root.h"
#include "GameEventListener.h"
#include "hudelement.h"
#include "gameui_interface.h"

class CCSGO_HudRadar;
class CCSGO_HudRadio;
class CCSGO_HudTeamCounter;
class CCSGO_HudUniqueAlerts;
class CCSGO_HudBlurTarget;

DECLARE_PANORAMA_EVENT1( PanoramaGameTimeJumpEvent, float );

class CPanoramaHudElement : public CHudElement
{
public:
	explicit CPanoramaHudElement(const char *pElementName, panorama::CPanel2D *pElementRootPanel );

	virtual void UpdateRepaintStateOnAncestors() OVERRIDE;

private:
	panorama::CPanel2D *m_pElementRootPanel;
};

//-----------------------------------------------------------------------------
// Purpose: CSGO Root Hud panel
//-----------------------------------------------------------------------------
class CCSGO_Hud : public CUI_Root, public ICSGOGameUIStateListener
{
	DECLARE_PANEL2D( CCSGO_Hud, CUI_Root );

public:
	CCSGO_Hud( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_Hud();

	static CCSGO_Hud *GetInstance() { return s_pHud; }

	virtual void OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState ) OVERRIDE;
	virtual void OnMapLoadFinished() OVERRIDE;

	void Update( void );

	// use of cl_hud_color
	void GetHudTextColor( Color *pColor );
	void GetBGHudTextColor( Color *pColor, const float flBrightness, const float flSaturation );

	void ReloadLayout();

	// elements that scale via hud_scaling, or affected by safezonex, safezoney
	panorama::CPanel2D *m_pHudTopLeft;
	panorama::CPanel2D *m_pHudTopCenter;
	panorama::CPanel2D *m_pHudTopRight;
	panorama::CPanel2D *m_pHudBottomRight;
	panorama::CPanel2D *m_pHudBottomCenter;
	panorama::CPanel2D *m_pHudLowerLeft;
	panorama::CPanel2D *m_pHudChat;
	panorama::CPanel2D *m_pHudSpectator;
	panorama::CPanel2D *m_pHudSpectatorScore;
	CCSGO_HudRadio *m_pHudRadio;
	CCSGO_HudTeamCounter *m_pHudTeamCounter;
	int m_nCachedClHudColor;

private:

	static CCSGO_Hud *s_pHud;

	void InitHud();

	void UpdateVisibility();
	void UpdateScaleAndSafezones();

	CCSGO_HudBlurTarget* m_pHudBlur;

};

