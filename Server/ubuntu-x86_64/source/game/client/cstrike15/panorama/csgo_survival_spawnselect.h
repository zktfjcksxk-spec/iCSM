//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Panorama menu for selecting spawn location in survival
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/controls/countdown.h"
#include "cs_gamerules_survival.h"

DECLARE_PANORAMA_EVENT1( SurvivalShowSpawnSelect, bool );
DECLARE_PANORAMA_EVENT0( SurvivalSpawnSelectUpdate );
DECLARE_PANORAMA_EVENT1( SurvivalSpawnSelectUpdateClockPrivate, int );

class CCSGO_SurvivalSpawnSelect : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_SurvivalSpawnSelect, panorama::CPanel2D );

public:
	explicit CCSGO_SurvivalSpawnSelect( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SurvivalSpawnSelect();

	// Event handlers
	bool HandleShow( bool bShow );
	bool HandleUpdate();
	bool HandleActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventLevelInitPostEntity();

	virtual void OnLayoutReloaded() OVERRIDE;

protected:
	enum TileState
	{
		kTileState_Enabled = 0, // default
		kTileState_Disabled,
		kTileState_SelectedByLocalPlayer,
		kTileState_SelectedByLocalPlayerTeammate,
		kTileState_SelectedByOtherPlayer,

		kTileState_Count
	};

	struct Tile
	{
		panorama::CPanelPtr<panorama::CPanel2D> m_pPanel;
		panorama::CPanelPtr<panorama::CButton> m_pButton;
		TileState m_TileState;
		int m_nTabletTile;
		int m_nNextTileForTablet; // linked list of tiles whose center is inside a particular tablet tile
	};

	enum TabletTileState
	{
		kTabletTileState_Empty = 0, // default
		kTabletTileState_Full,

		kTabletTileState_Count
	};

	struct TabletTile
	{
		panorama::CPanelPtr<panorama::CPanel2D> m_pPanel;
		int m_nFirstTile;
		TabletTileState m_TileState;
	};

	panorama::CPanelPtr<panorama::CImagePanel> m_pMapImage;
	panorama::CPanelPtr<panorama::CPanel2D> m_pMapLabelContainer;
	panorama::CPanelPtr<panorama::CPanel2D> m_pTabletTilePanel;
	panorama::CPanelPtr<panorama::CPanel2D> m_pTilePanel;
	panorama::CPanelPtr<panorama::CPanel2D> m_pPanelTimerBar;
	panorama::CPanelPtr<panorama::CPanel2D> m_pSpawnSelectLayout;
	CUtlVector<Tile> m_Tiles;
	CUtlVector<TabletTile> m_TabletTiles;
	int m_HighlightedTiles[MAX_PLAYERS];
	ESurvivalSpawnTileState m_SpawnTileStateShadow[SURVIVAL_SPAWN_TILE_NUM];
	CHandle<C_BasePlayer> m_hPlayer;

	// spawn data from gamerules
	int m_nSpawnStage;
	int m_nReady;

	// clock data
	float m_fStartTime;
	float m_fEndTime;
	int m_nClockVersion;
	bool HandleClockUpdate( int nClockVersion );

	void CreateSpawnTiles();
	void DestroySpawnTiles();

	void HandleTileActivated( int tileIdx );
	void SetTileState( int tileIdx, CSurvivalGameRules* pRules );
	void UpdateTabletTileState( int tabletTileIdx );

	uint64 m_hSpawnSelectInputHandler;
	void ReleaseInputHandler( void );
	void AddInputHandler( void );
};
