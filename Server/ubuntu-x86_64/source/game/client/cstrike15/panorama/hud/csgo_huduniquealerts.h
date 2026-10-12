//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"

#define MAX_PLAYER_GIFT_DROP_DISPLAY 4

//-----------------------------------------------------------------------------
// Purpose: Used to draw the history of weapon / item pickups and purchases by the player
//          and to show alert messages
//-----------------------------------------------------------------------------
class CCSGO_HudUniqueAlerts : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D(CCSGO_HudUniqueAlerts, panorama::CPanel2D);

public:
	CCSGO_HudUniqueAlerts(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_HudUniqueAlerts();

	// These overload the CHudElement class
	virtual void ProcessInput( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );

	// CGameEventListener methods
	virtual void FireGameEvent(IGameEvent *pEvent) OVERRIDE;

	virtual void OnTimeJump() OVERRIDE;

protected:
	// Calls either Show or Hide
	void ShowPanel( const bool bShow );
	void SetAlertStripVisible( bool bVisible );

private:
	void ShowAlert(char* szAlertText, int nAlertType = 0);

	// Show the item history
	void Show( void );
	// Hide the item history
	void Hide( void );

	void ShowWarmupAlertPanel( void );

	bool EventAnimationEnd(const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, panorama::CPanoramaSymbol symAnimation);

	panorama::CLabel * m_pAlertText;

	bool	m_bVisible;
	bool	m_bAlertStripVisible;				
	bool	m_bShowedFirstMsg;
	bool	m_bSuppressOneOffHide;
	float	m_flNextWarmupNoticeTick;

	bool	m_bWaitingForResume;
};
