//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "cs_gamerules_survival.h"
#include "c_info_map_region.h"

DECLARE_PANORAMA_EVENT1( ShowCompass, bool );
DECLARE_PANORAMA_EVENT0( CompassUpdate );

class CCSGO_Compass : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_Compass, panorama::CPanel2D );

public:
	explicit CCSGO_Compass( panorama::CPanel2D *pParent, const char *pchID );
	virtual void OnLayoutReloaded() OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void Think( void );

protected:

	float WorldPosToCompassPercentage( const Vector &worldPos );
	float CompassPosToOpacity( float flPercentage, float flSoften = 30.0f );
	bool SetRegionDialogVars( const Vector& vecPos );

	C_CSPlayer* GetCompassPlayer( void );

	struct compassPip
	{
		panorama::CPanelPtr<panorama::CPanel2D> m_pPanel;
		panorama::CPanelPtr<panorama::CLabel> m_pLabel;
		int m_nDegreeAngle;
		Vector m_vecWorldPosOffset;
		char m_szText[16];
	};
	CUtlVector<compassPip> m_Pips;

	struct compassTeammatePip
	{
		panorama::CPanelPtr<panorama::CPanel2D> m_pPanel;
		EHANDLE m_hTeammateHandle;
	};
	CUtlVector<compassTeammatePip> m_TeammatePips;
	void UpdateTeammatePips();
	void DestroyTeammatePips();
	void CreateTeammatePips();
	float m_flLastTeammateUpdateTime;

	struct compassRescuePip
	{
		panorama::CPanelPtr<panorama::CPanel2D> m_pPanel;
		Vector m_vecWorldPos;
	};
	CUtlVector<compassRescuePip> m_RescuePips;

	panorama::CPanelPtr< panorama::CLabel > m_pRegionName;
	CHandle< C_InfoMapRegion > m_pPreviousMapRegion;
	int m_nPreviousDirectionIdx = 0;

	EHANDLE m_hLastLocalPlayer;

	void CreatePips();
	void DestroyPips();

	void UpdateCompass( bool bVisible );
	bool m_bLasKnownVisibleState;
};
