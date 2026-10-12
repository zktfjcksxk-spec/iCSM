//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDRETICLE_H_
#define CSGO_HUDRETICLE_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?


struct CrosshairTargetIdRenderData_t;


//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Win Panel
//-----------------------------------------------------------------------------
class CCSGO_HudReticle : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudReticle, panorama::CPanel2D );

public:

	CCSGO_HudReticle( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudReticle();

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual void ProcessInput( void )  OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	enum ReticleMode_t
	{
		RETICLE_MODE_INVALID,	// Used to reset

		RETICLE_MODE_NONE,
		RETICLE_MODE_WEAPON,
		RETICLE_MODE_OBSERVER
	};

	// Event handlers
	bool OnShowTeamEquipment( bool value );

private:

	enum CrosshairPart_t
	{
		CROSSHAIR_PART_TOP,
		CROSSHAIR_PART_BOTTOM,
		CROSSHAIR_PART_LEFT,
		CROSSHAIR_PART_RIGHT,

		CROSSHAIR_PART_COUNT
	};

	struct PlayerIDSnippet_t
	{
		panorama::CPanel2D *m_pPlayerIDPanel;
		
		panorama::CPanel2D *m_pContentPanelParent;
		panorama::CPanel2D *m_pContentPanel;
		panorama::CPanel2D *m_pContentPanelTrans;
		panorama::CLabel *m_pNamePanel;
		panorama::CLabel *m_pNamePanelBG;
		panorama::CPanel2D *m_pWeaponsPanel;
		panorama::CImagePanel *m_pWeaponDefuserPanel;
		panorama::CImagePanel *m_pWeaponC4Panel;
		panorama::CPanel2D *m_pWeaponGrenadesPanel;
		panorama::CImagePanel *m_pWeaponPrimaryPanel;
		panorama::CPanel2D *m_pArrowFriendIconPanel;
		panorama::CPanel2D *m_pArrowIconPanel;
		panorama::CPanel2D *m_pArrowIconBorderPanel;
		panorama::CPanel2D *m_pChatIconPanel;
		panorama::CPanel2D *m_pDefusingIconPanel;
	};

	struct PlayerIDRenderData_t
	{
		void UpdateName( bool bShowIDName, bool bShouldShowAllFriendlyEquipment );
		void UpdateWeapons( bool bShouldShowAllFriendlyEquipment );
		
		EHANDLE m_hPlayer;
		int m_nTeam;

		bool m_bActive : 1;
		bool m_bNamePanelVisible : 1;
		bool m_bSetName : 1;
		bool m_bWeaponsPanelVisible : 1;
		bool m_bPrimaryVisible : 1;
		bool m_bSetPrimaryWeaponName : 1;
		bool m_bSetGrenadeWeaponName : 1;
		bool m_bDefuserVisible : 1;
		bool m_bC4Visible : 1;
		bool m_bIsFriend : 1;
		bool m_bVoiceActive : 1;
		bool m_bIsDefusing : 1;

		Vector m_vecWorldPos;	// World space position of the panel
		const char *m_pszName;
		int m_nHealth;			// value in the range [0..100], -1 if unset
		int m_nMoney;			// Positive value, -1 if unset
		CUtlString m_primaryWeaponName;
		CUtlString m_grenadeWeaponNames[5];
		int m_nGrenadesVisible;
		int m_nMaxGrenades;
		float m_flDesiredOpacity;
		float m_flOpacity;
		float m_flDesiredScale;
	};

	void ResetData();
	void ShowPanel( bool bShow );

	void GetLayoutDefines();
	bool OnStyleFileReloaded( panorama::CPanoramaSymbol symFile );

	bool AddPlayerID( CBaseEntity *player );
	void RemovePlayerID( int index );
	void RemoveAllPlayerID();

	// Helper functions collecting data used to update panorama panels
	void GetCrosshairTargetIdRenderData( CrosshairTargetIdRenderData_t &renderData );
	void GetPlayerTargetIDRenderData( CrosshairTargetIdRenderData_t &renderData, int iEntIndex, bool bFriendlyCrosshairOkay, bool bShowFriendEnemyDesignation, bool bCheckWeaponRange );
	void GetWeaponTargetIDRenderData( CrosshairTargetIdRenderData_t &renderData );
	void GetPlayerIDsRenderData();
	
	// Helper functions updating panorama panels
	void UpdateCrosshairUI( const CrosshairTargetIdRenderData_t &renderData );
	void UpdateCrosshairPartsPosition( panorama::CPanel2D *(&m_pPartsPanels)[CROSSHAIR_PART_COUNT], float flOffset );
	void UpdatePlayerIDsUI();

	bool ShouldShowAllFriendlyTargetIDs( void );
	bool ShouldShowAllFriendlyEquipment( void );

private:

	ReticleMode_t m_iReticleMode;
	int m_nCrosshairColorClassId;		// Value between [1..4]
	float m_flTargetIDTimer;
	float m_flWindowScaleFactor;
	CUtlString m_strCurrentTargetID;

	bool m_bForceShowAllTeammateTargetIDs;

	CUtlVector<PlayerIDRenderData_t> m_playerIDs;
	CUtlVector<PlayerIDSnippet_t> m_playerIDSnippets;

	// CSS Variables
	float m_flPipDefaultOffset;
	float m_flBlackPipDefaultOffset;
	float m_flArcDefaultOffset;
	float m_flTargetIDDuration;

	// Vector used to apply transfoms on pip & arc panels
	CUtlVector<panorama::CTransform3D *> m_vecTransforms;

	// Crosshair panels
	panorama::CPanel2D *m_pCrosshairPanel;
	panorama::CPanel2D *m_pCrosshairObserverPanel;
	panorama::CPanel2D *m_pArcPanels[CROSSHAIR_PART_COUNT];
	panorama::CPanel2D *m_pPipPanels[CROSSHAIR_PART_COUNT];
	panorama::CPanel2D *m_pBlackPipPanels[CROSSHAIR_PART_COUNT];
	panorama::CPanel2D *m_pFriendPanel;

	// TargetID panel
	panorama::CLabel *m_pTargetIDPanel;

	// PlayerID panel
	panorama::CPanel2D *m_pVisiblePlayerIDsPanel;
	panorama::CPanel2D *m_pUnusedPlayerIDsPanel;
};

#endif	// CSGO_HUDRETICLE_H_