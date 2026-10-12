//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_HUDRADIO_H_
#define CSGO_HUDRADIO_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?
#include "panorama/input/iuiinput.h"


struct sRadioCommand
{
	int m_hotKey;
	const char *m_label;
	const char *m_cmd;
};

struct sRadioCommandList
{
	int m_hotKey;
	const char *m_title;
	float m_flTimeout;

	CUtlVector<sRadioCommand> m_RadioCommands;
};

struct sRadioGroup
{
	panorama::CPanel2D *m_pGroupPanel;
	int	m_timerInterval;
};

//-----------------------------------------------------------------------------
// Purpose: Used to draw health and armor 
//-----------------------------------------------------------------------------
class CCSGO_HudRadio : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudRadio, panorama::CPanel2D );

public:

	CCSGO_HudRadio( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudRadio();

	// CHudElement overrides
	virtual void ProcessInput( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual bool OnKeyDown( const panorama::KeyData_t &unichar ) OVERRIDE;

	void ShowRadioGroup( int nSetID );

private:

	void ShowPanel( bool bShow );

private:

	static KeyValues *m_pKVRadioPanel;

	CUtlVector<sRadioCommandList> m_RadioCommandList;

	int				m_activeGroup;
	bool			m_bVisible;

	CountdownTimer m_CommandPanelTimer;

	panorama::CPanel2D *m_pRadioPanel;
	panorama::CPanel2D *m_pRadioPanelBG;
};

#endif	// CSGO_HUDRADIO_H_