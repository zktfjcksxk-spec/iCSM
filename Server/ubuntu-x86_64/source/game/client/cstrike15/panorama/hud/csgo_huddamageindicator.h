//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDDAMAGEINDICATOR_H_
#define CSGO_HUDDAMAGEINDICATOR_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?


//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Damage Indicator panel
//-----------------------------------------------------------------------------
class CCSGO_HudDamageIndicator : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudDamageIndicator, panorama::CPanel2D );

public:

	CCSGO_HudDamageIndicator( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudDamageIndicator();

	// CHudElement overrides
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void ProcessInput( void );
	virtual bool ShouldDraw( void );
	virtual void SetActive( bool bActive );

	// Handler for our message
	bool MsgFunc_Damage( const CCSUsrMsg_Damage &msg );

private:

	void ShowPanel( bool bShow );
	void ResetData();

	void CalcDamageDirection( const Vector &vecFrom, C_BasePlayer *pVictimPlayer );

private:

	CUserMessageBinder m_UMCMsgDamage;

	// Values between 0.0 and 1.0
	float m_flAttackFront;
	float m_flAttackRear;
	float m_flAttackLeft;
	float m_flAttackRight;

	float m_flLastFrameTime;

	// Panels
	panorama::CPanel2D *m_pDamagePanel;
	panorama::CPanel2D *m_pDamageTopPanel;
	panorama::CPanel2D *m_pDamageRightPanel;
	panorama::CPanel2D *m_pDamageBottomPanel;
	panorama::CPanel2D *m_pDamageLeftPanel;
};

#endif	// CSGO_HUDDAMAGEINDICATOR_H_