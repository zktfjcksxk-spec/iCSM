//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Panorama menu for selecting spawn location in survival mode
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_survival_spawnselect.h"
#include "IGameUIFuncs.h"
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientsteamcontext.h"
#include "cs_gamerules_survival.h"
#include "hudelement.h"
#include "c_info_map_region.h"
#include "uicomponents/uicomponent_gamestate.h"
#include "mathlib/hexes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_SurvivalSpawnSelect, CSGOSurvivalSpawnSelect );

DEFINE_PANORAMA_EVENT_DOC( SurvivalShowSpawnSelect, "bool", "Show or hide the spawn panel." );
DEFINE_PANORAMA_EVENT_DOC( SurvivalSpawnSelectUpdate, "", "Update state from survival gamerules" );
DEFINE_PANORAMA_EVENT( SurvivalSpawnSelectUpdateClockPrivate );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_SurvivalSpawnSelect::CCSGO_SurvivalSpawnSelect( panorama::CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/survival/survival_spawnselect.xml" );

	m_pMapImage = panorama::panel_cast< panorama::CImagePanel* >( FindChildInLayoutFile( "spawnselect-mapimage" ) );
	m_pMapLabelContainer = FindChildInLayoutFile( "spawnselect-map-labels" );
	m_pTilePanel = FindChildInLayoutFile( "spawnselect-tiles" );
	m_pTabletTilePanel = FindChildInLayoutFile( "spawnselect-tablet" );
	m_pPanelTimerBar = FindChildInLayoutFile( "timer-bar" );
	m_pSpawnSelectLayout = FindChildInLayoutFile( "spawnselect-layout" );
	memset( m_HighlightedTiles, -1, sizeof( m_HighlightedTiles ) );
	memset( m_SpawnTileStateShadow, 0x7f, sizeof( m_SpawnTileStateShadow ) );
	m_fStartTime = m_fEndTime = -1.0f;
	m_nClockVersion = 0;
	m_nSpawnStage = CSurvivalGameRules::SPAWN_STAGE_COUNT;
	m_hSpawnSelectInputHandler = 0;
	m_nReady = -1;

	SetVisible( false );
	RegisterForUnhandledEvent( SurvivalShowSpawnSelect(), this, &CCSGO_SurvivalSpawnSelect::HandleShow );
	RegisterForUnhandledEvent( SurvivalSpawnSelectUpdate(), this, &CCSGO_SurvivalSpawnSelect::HandleUpdate );
	RegisterForUnhandledEvent( GameState_OnLevelLoad(), this, &CCSGO_SurvivalSpawnSelect::EventLevelInitPostEntity );
	RegisterEventHandler( Activated(), this, &CCSGO_SurvivalSpawnSelect::HandleActivated );
	RegisterEventHandler( SurvivalSpawnSelectUpdateClockPrivate(), this, &CCSGO_SurvivalSpawnSelect::HandleClockUpdate );
}

void CCSGO_SurvivalSpawnSelect::ReleaseInputHandler( void )
{
	if ( m_hSpawnSelectInputHandler )
	{
		gameuifuncs->PanoramaReleaseGameInputHandler( m_hSpawnSelectInputHandler );
		m_hSpawnSelectInputHandler = 0;
	}
}

void CCSGO_SurvivalSpawnSelect::AddInputHandler( void )
{
	ReleaseInputHandler();
	m_hSpawnSelectInputHandler = gameuifuncs->PanoramaAddGameInputHandler( this->UIPanel(), panorama::k_EGameInputShareMouse, "SurvivalSpawnSelect" );
}

CCSGO_SurvivalSpawnSelect::~CCSGO_SurvivalSpawnSelect()
{
	ReleaseInputHandler();
}

void CCSGO_SurvivalSpawnSelect::CreateSpawnTiles()
{
	DestroySpawnTiles();

	CCSGameRules* pGameRules = CSGameRules();
	CSurvivalGameRules* pBRRules = pGameRules ? pGameRules->GetSurvivalRules() : nullptr;
	if ( !pBRRules )
		return;

	AABB_t mapArea = pBRRules->GetPlayAreaBounds();

	if ( CPanel2D* pTabletTilesPanel = m_pTabletTilePanel.Get() )
	{
		const int nNumTabletHexes = WORLD_HEX_NUM;

		Vector2D hexSize( pBRRules->GetTabletHexSize(), pBRRules->GetTabletHexSize() );
		hexSize /= mapArea.GetSize().AsVector2D();
		hexSize.y *= kHexHeight;

		Vector sizeAsPercent = Vector( hexSize.x * 100.0f, hexSize.y * 100.0f, 0 );

		for ( int i = 0; i < nNumTabletHexes; ++i )
		{
			// make sure indices line up
			Assert( m_TabletTiles.Count() == i );

			CPanel2D* pTilePanel = new CPanel2D( pTabletTilesPanel, nullptr );
			pTilePanel->RequireLoadLayoutSnippet( "spawnselect-tablet-hex" );

			// Layout hexes
			Vector2D vecPosition = pBRRules->GetHexCenter( i );
			vecPosition -= mapArea.m_vMinBounds.AsVector2D();
			vecPosition /= mapArea.GetSize().AsVector2D();

			// we seem to be inverting the hexes?
			vecPosition.y = 1.0f - vecPosition.y;

			// scale to %
			vecPosition *= 100.0f;

			// offset by half hex size so we are placing top left and not center
			vecPosition -= sizeAsPercent.AsVector2D() * 0.5f;

			pTilePanel->SetPosition(
				CUILength( vecPosition.x, CUILength::k_EUILengthPercent ),
				CUILength( vecPosition.y, CUILength::k_EUILengthPercent ),
				CUILength( 0, CUILength::k_EUILengthLength ) );
			pTilePanel->SetSize(
				CUILength( sizeAsPercent.x, CUILength::k_EUILengthPercent ),
				CUILength( sizeAsPercent.y, CUILength::k_EUILengthPercent ) );

			TabletTile tile;
			tile.m_pPanel = pTilePanel;
			tile.m_nFirstTile = -1;
			tile.m_TileState = kTabletTileState_Count;
			m_TabletTiles.AddToTail( tile );
		}
	}

	if ( CPanel2D* pTilesPanel = m_pTilePanel.Get() )
	{
		const int nNumHexes = pBRRules->GetNumSpawnHex();
		Vector size = Vector( pBRRules->GetSpawnHexWidth(), pBRRules->GetSpawnHexHeight(), 0 );

		for ( int i = 0; i < nNumHexes; ++i )
		{
			// make sure indices line up
			Assert( m_Tiles.Count() == i );

			CPanel2D* pTilePanel = new CPanel2D( pTilesPanel, nullptr );
			pTilePanel->RequireLoadLayoutSnippet( "spawnselect-map-hex" );

			// Layout hexes
			Vector position = pBRRules->Get2DHexPosition( i );
			pTilePanel->SetPosition(
				CUILength( position.x * 100.0f, CUILength::k_EUILengthPercent ),
				CUILength( position.y * 100.0f, CUILength::k_EUILengthPercent ),
				CUILength( 0, CUILength::k_EUILengthLength ) );
			pTilePanel->SetSize(
				CUILength( size.x * 100.0f, CUILength::k_EUILengthPercent ),
				CUILength( size.y * 100.0f, CUILength::k_EUILengthPercent ) );

			// Find the first button child
			CButton* pButton = nullptr;
			pTilePanel->IterateChildrenTraverseOfType<panorama::CButton>( [&]( panorama::CButton* pChild ) { pButton = pChild; return false; } );

			Tile tile;
			tile.m_pPanel = pTilePanel;
			tile.m_pButton = pButton;
			tile.m_TileState = kTileState_Count; // force re-initialization in SetTileState

			// Figure out tablet tile for this tile
			Vector vecWorldPosition = pBRRules->GetSpawnHexWorldPosition( i );
			int iTabletHex = pBRRules->FindGridCenterIndexClosestToWorldPos( vecWorldPosition );
			if ( iTabletHex >= 0 && iTabletHex < m_TabletTiles.Count() )
			{
				TabletTile& tabletTile = m_TabletTiles[iTabletHex];

				tile.m_nTabletTile = iTabletHex;

				// insert into linked list of tiles that use this tablet hex
				tile.m_nNextTileForTablet = tabletTile.m_nFirstTile;
				tabletTile.m_nFirstTile = i;
			}
			else
			{
				tile.m_nTabletTile = -1;
			}

			m_Tiles.AddToTail( tile );
		}

		// Set initial states from rules
		FOR_EACH_VEC( m_Tiles, i )
		{
			SetTileState( i, pBRRules );
		}

		// Tablet tiles that don't contain any spawn tiles won't have been updated by this loop.
		// Get them to their 'empty' state (which won't ever be updated after this)
		FOR_EACH_VEC( m_TabletTiles, i )
		{
			if ( m_TabletTiles[i].m_nFirstTile < 0 )
				UpdateTabletTileState( i );
		}

		memcpy( m_HighlightedTiles, pBRRules->GetSpawnSelectIndices(), sizeof( m_HighlightedTiles ) );
		memcpy( m_SpawnTileStateShadow, pBRRules->GetSpawnTileStates(), sizeof( m_SpawnTileStateShadow ) );
	}
}

void CCSGO_SurvivalSpawnSelect::DestroySpawnTiles()
{
	FOR_EACH_VEC( m_TabletTiles, i )
	{
		TabletTile& tile = m_TabletTiles[i];

		if ( CPanel2D* pPanel = tile.m_pPanel.Get() )
		{
			tile.m_pPanel = nullptr;
			delete pPanel;
		}
	}

	m_TabletTiles.RemoveAll();

	FOR_EACH_VEC( m_Tiles, i )
	{
		Tile& tile = m_Tiles[i];

		if ( CPanel2D* pPanel = tile.m_pPanel.Get() )
		{
			tile.m_pPanel = nullptr;
			delete pPanel;
		}
	}

	m_Tiles.RemoveAll();
}

bool CCSGO_SurvivalSpawnSelect::HandleShow( bool bShow )
{
	if ( BIsVisible() != bShow )
	{
		//Control the dropzone audio loop playing when in the drop zone screen.
		C_BasePlayer* pLocalPlayer = CHudElement::GetLocalOrObservedPlayer(0);

		if( pLocalPlayer != nullptr )
		{
			if ( bShow )
			{
				pLocalPlayer->EmitSound( "Survival.DropzoneBGLoop" );
			}
			else
			{
				pLocalPlayer->StopSound( "Survival.DropzoneBGLoop" );
			}
		}
	}

	// Load map
	if ( m_pMapImage )
	{
		m_pMapImage->SetImage( CFmtStr( "file://{images}/survival/spawnselect/map_%s.png", engine->GetLevelNameShort() ) );
	}

	SetVisible( bShow );

	if ( bShow )
	{
		AddInputHandler();

		CreateSpawnTiles();
		m_fStartTime = m_fEndTime = -1.0f;
		HandleUpdate();

		if ( m_pSpawnSelectLayout )
		{
			m_pSpawnSelectLayout->AddClass( "spawnselect--PopOut" );
			this->RemoveClass( "spawnselect--FadeOut" );
		}

	}
	else
	{
		ReleaseInputHandler();

		DestroySpawnTiles();
		++m_nClockVersion; // kill clock event

		if ( m_pSpawnSelectLayout )
		{
			m_pSpawnSelectLayout->RemoveClass( "spawnselect--PopOut" );
		}
	}

	return false;
}

bool CCSGO_SurvivalSpawnSelect::HandleUpdate()
{
	if ( !CSGameRules() || !CSGameRules()->GetSurvivalRules() )
		return false;

	CSurvivalGameRules* pSurvivalRules = CSGameRules()->GetSurvivalRules();
	float fStartTime = pSurvivalRules->GetSpawnSelectTimeStart();
	float fEndTime = pSurvivalRules->GetSpawnSelectTimeStageEnd();
	const int* pSelectedTiles = pSurvivalRules->GetSpawnSelectIndices();
	const ESurvivalSpawnTileState* pSurvivalTileStates = pSurvivalRules->GetSpawnTileStates();
	CSurvivalGameRules::SpawnStage_t spawnStage = pSurvivalRules->GetSpawnStage();
	C_BasePlayer* pLocalPlayer = CHudElement::GetLocalOrObservedPlayer( 0 );
	bool bChangedPlayer = m_hPlayer.ChangedFrom( pLocalPlayer );
	m_hPlayer = pLocalPlayer;

	if ( spawnStage == CSurvivalGameRules::SPAWN_STAGE_NONE || fStartTime < 0 || !pLocalPlayer )
	{
		// Not in spawn select period
		if ( BIsVisible() )
			HandleShow( false );

		return false;
	}

	if ( m_pSpawnSelectLayout && spawnStage == CSurvivalGameRules::SPAWN_STAGE_ALL_READY )
	{
		if ( gpGlobals->curtime + 1.5f > fEndTime )
		{
			this->AddClass( "spawnselect--FadeOut" );
		}
	}

	if ( !BIsVisible() )
	{
		HandleShow( true );  // TODO: Put a cool transition here on mode start!
	}

	// Update hex states
	COMPILE_TIME_ASSERT( sizeof( pSelectedTiles[0] ) == sizeof( m_HighlightedTiles[0] ) );
	COMPILE_TIME_ASSERT( V_ARRAYSIZE( m_HighlightedTiles ) == MAX_PLAYERS ); // V_ARRAYSIZE(pSelectedTiles)

	bool bAllDirty = bChangedPlayer;
	if ( m_nSpawnStage != spawnStage )
	{
		const char* kSpawnClasses[] = {
			nullptr,
			"spawnselect--stage-selection",
			"spawnselect--stage-ready",
			"spawnselect--stage-locked",
		};
		const char* kSpawnTitles[] = {
			"",
			"#Survival_SpawnSelect_ChooseDeployment",
			"#Survival_SpawnSelect_DeploymentLocked",
			"#Survival_SpawnSelect_DeploymentLocked",
		};
		COMPILE_TIME_ASSERT( V_ARRAYSIZE( kSpawnClasses ) == CSurvivalGameRules::SPAWN_STAGE_COUNT );
		COMPILE_TIME_ASSERT( V_ARRAYSIZE( kSpawnTitles ) == CSurvivalGameRules::SPAWN_STAGE_COUNT );

		const char* szSpawnStageClass = nullptr;
		if ( spawnStage >= 0 && spawnStage < CSurvivalGameRules::SPAWN_STAGE_COUNT )
		{
			szSpawnStageClass = kSpawnClasses[spawnStage];
			SetDialogVariableLocString( "title", kSpawnTitles[spawnStage] );
		}

		SwitchClass( "spawnselect--stage", szSpawnStageClass );
		m_nSpawnStage = spawnStage;

		// need to update tiles if spawn stage changes
		bAllDirty = true;
	}

	int nTiles = m_Tiles.Count();

	// Need to update all tiles if you change the observed player
	if ( pLocalPlayer && m_HighlightedTiles[pLocalPlayer->entindex() - 1] != pSelectedTiles[pLocalPlayer->entindex() - 1] )
	{
		// Also need to update all tiles if the local player's selection changes
		bAllDirty = true;
	}

	for ( int i = 0; i < nTiles; ++i )
	{
		if ( bAllDirty || m_SpawnTileStateShadow[i] != pSurvivalTileStates[i] )
		{
			m_SpawnTileStateShadow[i] = pSurvivalTileStates[i];
			SetTileState( i, pSurvivalRules );
		}
	}

	for ( int i = 0; i < MAX_PLAYERS; ++i )
	{
		int oldTile = m_HighlightedTiles[i];
		int newTile = pSelectedTiles[i];

		if ( oldTile != newTile )
		{
			m_HighlightedTiles[i] = newTile;
			SetTileState( oldTile, pSurvivalRules );
			SetTileState( newTile, pSurvivalRules );
		}
	}

	int nReady = 0;
	int nTotal = 0;
	for ( int i = 0; i < MAX_PLAYERS; ++i )
	{
		int iPlayerEntIndex = i + 1;
		if ( GetCSResources() && GetCSResources()->IsConnected( iPlayerEntIndex ) && GetCSResources()->IsAlive( iPlayerEntIndex ) )
		{
			int teamNum = GetCSResources()->GetTeam( iPlayerEntIndex );
			if ( teamNum >= 2 ) // not a spectator
			{
				nTotal++;

				if ( pSelectedTiles[i] >= 0 )
					nReady++;
			}
		}
	}

	if ( nReady != m_nReady )
	{
		m_nReady = nReady;
		SetDialogVariable( "spawnselect-player-select-count", nReady );
		SetDialogVariable( "spawnselect-player-total-count", nTotal );

		SetHasClass( "spawnselect--local-player-ready", !pLocalPlayer || pSelectedTiles[pLocalPlayer->entindex() - 1] >= 0 );
	}

	if ( m_fStartTime != fStartTime || m_fEndTime != fEndTime )
	{
		// Update clock
		m_fStartTime = fStartTime;
		m_fEndTime = fEndTime;
		HandleClockUpdate( ++m_nClockVersion );
	}


	return false;
}

// TODO: genericize clock
bool CCSGO_SurvivalSpawnSelect::HandleClockUpdate( int nClockVersion )
{
	if ( nClockVersion != m_nClockVersion )
		return true;

	if ( m_fStartTime < 0 )
	{
		SetDialogVariable( "timer-seconds", 0 );
		SetHasClass( "spawnselect--timer-invalid", true );
		return true;
	}

	float fTimeRemainingSeconds = Max( m_fEndTime - gpGlobals->curtime, 0.0f );
	int nTimeRemainingSeconds = Floor2Int( fTimeRemainingSeconds );
	SetDialogVariable( "timer-seconds", nTimeRemainingSeconds );
	SetHasClass( "spawnselect--timer-invalid", false );

	if ( panorama::CPanel2D* pPanelTimerBar = m_pPanelTimerBar.Get() )
	{
		float fDuration = m_fEndTime - m_fStartTime;
		float fOffset = gpGlobals->curtime - m_fStartTime;

		static ConVarRef host_timescale( "host_timescale" );
		float flTimeScale = host_timescale.GetFloat();
		if ( engine->IsPaused() || flTimeScale < 0.0001f )
		{
			// "pause" by running very slowly
			flTimeScale = 0.0001f;
		}

		fDuration /= flTimeScale;
		fOffset /= flTimeScale;

		// For some reason CPanel2D has BSetProperty protected.  End-around by upcasting to the interface where it's public
		IUIPanelClient* pTimerPanel = pPanelTimerBar;
		pTimerPanel->BSetProperty( "style", CFmtStr( "animation-duration: %.2fs; animation-delay: %.2fs;", fDuration, -fOffset ) );
	}
	
	// Figure out how long until the next text update and update the clock when that time has passed
	// TODO: Handle time scale here?
	float fTimeUntilNextUpdate = fTimeRemainingSeconds - nTimeRemainingSeconds;
	panorama::DispatchEventAsync( fTimeUntilNextUpdate, SurvivalSpawnSelectUpdateClockPrivate(), this, m_nClockVersion );

	return true;
}

bool CCSGO_SurvivalSpawnSelect::HandleActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanelHandle, panorama::EPanelEventSource_t eSource )
{
	// always return false to let other things handle the activation if needed

	panorama::IUIPanel* pPanel = pPanelHandle.Get();
	if ( !pPanel )
		return false;

	panorama::IUIPanelClient* pClientPanel = pPanel->ClientPtr();
	if ( !pClientPanel )
		return false;

	FOR_EACH_VEC( m_Tiles, i )
	{
		Tile& tile = m_Tiles[i];

		if ( pClientPanel == tile.m_pPanel.Get() || pClientPanel == tile.m_pButton.Get() )
		{
			HandleTileActivated( i );
			break;
		}
	}

	return false;
}

void CCSGO_SurvivalSpawnSelect::HandleTileActivated( int tileIdx )
{
	if ( tileIdx < 0 || tileIdx >= m_Tiles.Count() )
		return;

	engine->ServerCmd( CFmtStr( "dz_spawnselect_choose_hex %d;", tileIdx ) );
}

void CCSGO_SurvivalSpawnSelect::SetTileState( int tileIdx, CSurvivalGameRules* pSurvivalRules )
{
	if ( tileIdx < 0 || tileIdx >= m_Tiles.Count() )
		return;

	Tile& tile = m_Tiles[tileIdx];

	// figure out the desired state
	TileState state = kTileState_Enabled; // default

	if ( pSurvivalRules )
	{
		const int* pSelectedTiles = pSurvivalRules->GetSpawnSelectIndices();
		const ESurvivalSpawnTileState* pSpawnTileStates = pSurvivalRules->GetSpawnTileStates();
		ESurvivalSpawnTileState eSpawnTileState = pSpawnTileStates[tileIdx];

		// Unlike most HUD, when spectating spawn select it's very unclear who you are actually
		// spectating.  So don't show their selections in the HUD specially.
		//
		// TODO: If we detect that you are spectating in team mode, we could draw the teams
		//       as different colors like we do in csgo_mapoverview
		//
		//C_BasePlayer* pLocalPlayer = CHudElement::GetLocalOrObservedPlayer( 0 );
		C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer( 0 );

		int localPlayerIndex = -1;
		if ( pLocalPlayer )
			localPlayerIndex = pLocalPlayer->entindex() - 1;

		int localTile = -1;
		if ( localPlayerIndex >= 0 && localPlayerIndex < MAX_PLAYERS )
			localTile = pSelectedTiles[localPlayerIndex];

		// If the user has selected a tile, the remaining tiles default to disabled
		if ( localTile >= 0 && eSpawnTileState == kSurvivalSpawn_Available )
			eSpawnTileState = kSurvivalSpawn_UI_Invalid; // make invisible

		switch ( eSpawnTileState )
		{
		case kSurvivalSpawn_Available:
			if ( pSurvivalRules->GetSpawnStage() == CSurvivalGameRules::SPAWN_STAGE_SELECTION )
				state = kTileState_Enabled;
			else
				state = kTileState_Disabled;
			break;

		case kSurvivalSpawn_GameBlocked:
		case kSurvivalSpawn_MapBlocked:		// TODO: Shade red?  We don't currently use this state
			state = kTileState_Disabled;
			break;

		case kSurvivalSpawn_UI_Invalid:
			// Just make these invisible too
			state = kTileState_Disabled;
			break;

		case kSurvivalSpawn_Occupied:
		case kSurvivalSpawn_TempLocked:
		case kSurvivalSpawn_Locked:
			// Currently we don't distinguish these in the UI
			if ( localTile == tileIdx )
				state = kTileState_SelectedByLocalPlayer;
			else
			{
				PlayerTeammateVector_t teammates;
				pSurvivalRules->GetPlayerTeammates( ToCSPlayer( pLocalPlayer ), teammates );

				bool bIsTeammateTile = false;
				FOR_EACH_VEC( teammates, iTeammate )
				{
					int teammatePlayerIndex = teammates[iTeammate]->entindex() - 1;
					int teammateTileIndex = pSelectedTiles[teammatePlayerIndex];
					if ( teammateTileIndex == tileIdx )
					{
						bIsTeammateTile = true;
						break;
					}
				}

				state = bIsTeammateTile ? kTileState_SelectedByLocalPlayerTeammate : kTileState_SelectedByOtherPlayer;
			}
			break;

		default:
			state = kTileState_Disabled; // shouldn't happen
		}
	}

	if ( state != tile.m_TileState )
	{
		static const char * const kStates[] = {
			"tile--enabled",
			"tile--disabled",
			"tile--selected-local-player",
			"tile--selected-local-teammate",
			"tile--selected-other-player",
		};
		COMPILE_TIME_ASSERT( V_ARRAYSIZE( kStates ) == kTileState_Count );

		tile.m_pPanel->SwitchClass( "tile--state", ( state >= 0 && state < kTileState_Count ) ? kStates[state] : nullptr );
		tile.m_TileState = state;

		UpdateTabletTileState( tile.m_nTabletTile );
	}
}

void CCSGO_SurvivalSpawnSelect::UpdateTabletTileState( int tabletTileIdx )
{
	if ( tabletTileIdx < 0 || tabletTileIdx >= m_TabletTiles.Count() )
		return;

	TabletTile& tabletTile = m_TabletTiles[tabletTileIdx];
	TabletTileState state = kTabletTileState_Empty;

	int iTile = tabletTile.m_nFirstTile;
	while ( iTile >= 0 && iTile < m_Tiles.Count() )
	{
		Tile& tile = m_Tiles[iTile];

		switch ( tile.m_TileState )
		{
		case kTileState_Enabled:
		case kTileState_Disabled:
			break;

		case kTileState_SelectedByLocalPlayer:
		case kTileState_SelectedByLocalPlayerTeammate:
		case kTileState_SelectedByOtherPlayer:
			state = kTabletTileState_Full;
			break;

		case kTileState_Count:
			// not initialized yet, assume empty
			break;

		default:
			Assert( 0 );
			break;
		}

		iTile = tile.m_nNextTileForTablet;
	}

	if ( state != tabletTile.m_TileState )
	{
		static const char * const kStates[] = {
			"tablet-tile--empty",
			"tablet-tile--full",
		};
		COMPILE_TIME_ASSERT( V_ARRAYSIZE( kStates ) == kTabletTileState_Count );

		tabletTile.m_pPanel->SwitchClass( "tablet-tile--state", ( state >= 0 && state < kTabletTileState_Count ) ? kStates[state] : nullptr );
		tabletTile.m_TileState = state;
	}
	
}


//-----------------------------------------------------------------------------
// Purpose: Handle hotloading of our panel data
//-----------------------------------------------------------------------------
void CCSGO_SurvivalSpawnSelect::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();

	SetVisible( false );
	HandleUpdate();
}

bool CCSGO_SurvivalSpawnSelect::EventLevelInitPostEntity()
{
	m_pMapLabelContainer->RemoveAndDeleteChildren();

	CSurvivalGameRules* pRules = CSGameRules()->GetSurvivalRules();
	if ( !pRules )
		return false;

	panorama::CPanel2D* pMapLabelContainer = m_pMapLabelContainer.Get();
	if ( !pMapLabelContainer )
		return false;

	AABB_t playArea = pRules->GetPlayAreaBounds();
	Vector vecWorldCenter = playArea.GetCenter();
	float flWorldWidth = playArea.GetSize().x;
	float flWorldHeight = playArea.GetSize().y;

	for ( C_InfoMapRegion* pEnt = GetRegionNameList(); pEnt != nullptr; pEnt = pEnt->m_pNext )
	{
		if ( pEnt->m_szLocToken && !StringIsEmpty( pEnt->m_szLocToken ) )
		{
			panorama::CPanel2D* pLocationEntry = new panorama::CPanel2D( pMapLabelContainer, nullptr );
			pLocationEntry->BLoadLayoutSnippet( "spawnselect-map-label" );

			panorama::CLabel* pLabel = panorama::panel_cast< panorama::CLabel* >( pLocationEntry->FindChildInLayoutFile( "map-location-label" ) );
			if ( pLabel )
				pLabel->SetLocalizationString( pEnt->m_szLocToken );

			Vector vMapPos = pEnt->GetAbsOrigin() - vecWorldCenter;
			CUILength x( ( flWorldWidth / 2 + vMapPos.x ) / flWorldWidth * 100.f, CUILength::k_EUILengthPercent );
			CUILength y( ( flWorldHeight / 2 - vMapPos.y ) / flWorldHeight * 100.f, CUILength::k_EUILengthPercent );
			pLocationEntry->AccessStyle()->SetPosition( x, y, CUILength( 0, CUILength::k_EUILengthLength ) );
		}
	}

	return false;
}


