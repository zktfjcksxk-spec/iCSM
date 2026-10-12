//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Survival panel showed during end-of-match screen.
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"

DECLARE_PANORAMA_EVENT0( CSGOSurvivalStatsUpdated );

class CCSGO_SurvivalEndOfMatch : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_SurvivalEndOfMatch, panorama::CPanel2D );

public:
	CCSGO_SurvivalEndOfMatch( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SurvivalEndOfMatch();

	void SetupJavascriptObjectTemplate() OVERRIDE;

	// JS API
	v8::Local<v8::Value> GetInfo();
	void SetPlayerInQueueButtonStates();
	void UpdateButtonStates();

public:
	static bool IsInQueue();
	static char const * CanRequeueSoloError();
	static char const * CanRequeueDuosError();
	static bool RequestSurvivalEndOfMatchDisconnect( uint32 unValue );


private:
	panorama::CPanelPtr<panorama::CButton> m_pSpectateButton;
	panorama::CPanelPtr<panorama::CButton> m_pQueueButton;
	panorama::CPanelPtr<panorama::CPanel2D> m_pWarningContainer;
	panorama::CPanelPtr<panorama::CLabel> m_pWarningLabel;
	double m_flLastButtonUpdate;

	bool OnEndOfMatchSurvivalRequeueClicked();
	bool EventCSGOFrameUpdate();
};
