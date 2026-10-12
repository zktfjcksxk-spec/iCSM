//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Spectator hud panel
//
//=============================================================================//

#include "cbase.h"
#include "panorama/iuiengine.h"
#include "csgo_hudspecplayer.h"
#include "c_cs_playerresource.h"
#include "csgo_hudfreezepanel.h"
#include "c_team.h"
#include "panorama/uievents.h"
#include "hltvreplaysystem.h"
#include <filesystem.h>
#include "../utils/common/filesystem_tools.h"
//#include "uicomponents/uicomponent_friendslist.h"
//#include "uicomponents/uicomponent_inventory.h"
//#include "uicomponents/uicomponent_loadout.h"
//#include "uicomponents/uicomponent_mypersona.h"
//#include "uicomponents/uicomponent_itemdata.h"
//#include "uicomponents/uicomponent_matchstats.h"
#include "uicomponents/uicomponent_gamestate.h"
#include "clientmode_csnormal.h" // CSGOFrameUpdate
#include "bone_setup.h"

#include "cdll_util.h"

// To get team counter panel instance as it has some spectator related logic in it...
#include "hud.h"
#include "panorama/hud/csgo_hudteamcounter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_HudSpecPlayer, CSGOHudSpecPlayer );

extern ConVar spec_cameraman_xray;
extern ConVar spec_autodirector;
extern ConVar spec_autodirector_cameraman;
extern ConVar spec_hide_players;
extern ConVar cl_spec_show_bindings;
extern ConVar cl_server_graphic1_enable;
extern ConVar sv_server_graphic1;
extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;

CCSGO_HudSpecPlayer::CCSGO_HudSpecPlayer( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_HudSpecPlayer", this ),
	CPanel2D( pParent, pchID ),
	m_colorForAvatarPlayer( 0, 0, 0, 0 ),
	m_nTargetID( 0 ),
	m_iLastTargetID( 0 ),
	m_iLastTargetControlledByID( 0 ),
	m_iLastHealth( 0 ),
	m_iLastArmor( 0 ),
	m_fNextUpdateTime( 0 ),
	m_iLastItemId( 0 ),
	m_iLastDisplayItemId( 0 ),
	m_bReplayPlayerTagVisible( false ),
	m_bCallShowReplayUI( false )
{
	SetInputNamespace( "CSGOHudSpecPlayer" );
	RequireLoadLayout( "file://{resources}/layout/hud/hudspecplayer.xml" );
	m_pPlayerAvatar				= panorama::panel_cast<CCSGO_AvatarImage*> ( FindChildInLayoutFile( "PlayerAvatar" ) );
	m_pPlayerAvatarColor		= FindChildInLayoutFile( "PlayerAvatarColor" );
	m_pPlayerAvatarDefault		= panorama::panel_cast<panorama::CImagePanel*> ( FindChildInLayoutFile( "PlayerAvatarDefault" ) );
	m_pPlayerAvatarDisplayItem	= panorama::panel_cast<panorama::CImagePanel*>( RequireChildInLayoutFile( "PlayerAvatarDisplayItem" ) );
	
	m_pDamageTakenPanel = panel_cast< CLabel* > ( FindChildInLayoutFile( "Stat_DamageTaken" ) );
	m_pDamageGivenPanel = panel_cast< CLabel* > ( FindChildInLayoutFile( "Stat_DamageGiven" ) );
	
	m_pStatsLabelContainer = RequireChildInLayoutFile( "HudSpecplayer__Stats" );

	m_pStat_HeadX.SetSize( 5 );
	for ( int i = 0; i < 5; i++ )
		m_pStat_HeadX[i] = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( CFmtStr( "Stat_HeadX_%d", i + 1 ) ) );

	m_pStat_NumX.SetSize( 5 );
	for ( int i = 0; i < 5; i++ )
		m_pStat_NumX[i] = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( CFmtStr( "Stat_NumX_%d", i + 1 ) ) );

	m_pHotKeyLabelContainer = RequireChildInLayoutFile( "HotKeyLabelContainer" );
	m_pHotKeys[HOTKEY_TYPE_CONTROLBOT]		= RequireChildInLayoutFile( "HotKey_ControlBot" );
	m_pHotKeys[HOTKEY_TYPE_XRAY]			= RequireChildInLayoutFile( "HotKey_XRay" );
	m_pHotKeys[HOTKEY_TYPE_CANCELREPLAY]	= RequireChildInLayoutFile( "HotKey_CancelReplay" );
	m_pHotKeys[HOTKEY_TYPE_ARROWS]			= RequireChildInLayoutFile( "HotKey_Arrows" );
	m_pHotKeys[HOTKEY_TYPE_CAMERA]			= RequireChildInLayoutFile( "HotKey_Camera" );
	m_pHotKeys[HOTKEY_TYPE_MAP]				= RequireChildInLayoutFile( "HotKey_Map" );
	m_pHotKeys[HOTKEY_TYPE_CAMERAMANON]		= RequireChildInLayoutFile( "HotKey_CameramanOn" );
	m_pHotKeys[HOTKEY_TYPE_DIERCTORON]		= RequireChildInLayoutFile( "HotKey_DirectorOn" );
	m_pHotKeys[HOTKEY_TYPE_REPLAYLASTKILL]	= RequireChildInLayoutFile( "HotKey_ReplayLastKill" );

	m_pWeaponImageContainer = RequireChildInLayoutFile( "ItemContainerBG" );
//	m_pWeaponImagePanel		= panorama::panel_cast<panorama::CImagePanel*>( RequireChildInLayoutFile( "WeaponImage" ) );
	m_pWeaponNameLabel		= panel_cast< CLabel* > ( FindChildInLayoutFile( "WeaponName" ) );
	m_pDefaultTeamLogo		= RequireChildInLayoutFile( "DefaultTeamLogo" );
	m_pTeamLogo				= RequireChildInLayoutFile( "TeamLogo" );
	m_pServerSponserLogos	= panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "ServerSponserLogos" ) );

	m_pColorStrip			= RequireChildInLayoutFile( "PlayerColorStrip" );

	m_pReplayPlayerTag		= RequireChildInLayoutFile( "ReplayPlayerTag" );

	//SetLastInputTime( 0 );

	m_nDamageGiven = 0;
	m_nDamageTaken = 0;

	ListenForGameEvent( "spec_target_updated" );
	ListenForGameEvent( "cs_prev_next_spectator" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "show_freezepanel" );

}

CCSGO_HudSpecPlayer::~CCSGO_HudSpecPlayer()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudSpecPlayer::ProcessInput( void )
{
	//UpdateReplayUI();
}

void CCSGO_HudSpecPlayer::FireGameEvent( IGameEvent *evt )
{
	// todo, add team change event
	const char* szEventName = evt->GetName();
	if ( FStrEq( "spec_target_updated", szEventName ) ||
		 FStrEq( "round_start", szEventName ) )
	{
		// Server (or hltvcamera) changed our target, update info on the display panel
		m_iLastTargetID = -1;
		UpdateSpectatedPlayer();
	}
	else if ( FStrEq( "cs_prev_next_spectator", szEventName ) )
	{
		// LEGACY: Ok, so old behavior was to ask sfhud_teamcounter for rules about which player we should observe next and send a 
		// spec_player for that target to the server/hltv, or just send spec_next/spec_prev if there was some failure in that teamcounter code. 
		// Server (at the time of this writing) does nothing with spec_next/spec_prev besides update a timestamp used by the autokick feature. 
		// The HLTVDirector, however, does use spec_nextd/prev to cycle targets, so sending it as a fallback may be important to maintain.
		// We are not going to refactor any server code for the panorama port, so if the intention is to update that timestamp 
		// when the observer demonstrates they're still around, we should send a spec_next to bump that autokick time as well as a command to
		// change our spec target. Trying to reproduce the old sfhud_teamcounter logic below since spectating code should probably live here and sending up
		// a spec_next just to bump the timestamp.
		// NOTE: Reverse is flipped: 'next' is true when attack2 is pressed, false when +attack is pressed.
		bool bReverse = evt->GetBool( "next" );
		int idx = FindNextSpectatorTarget( bReverse );
		if ( idx > 0 )
		{
			engine->ClientCmd( CFmtStr( "spec_player %d", idx ).Get() );
		}
		else
		{
			// These actually change the target with HLTVDirector, but only update kick timestamp on a server... 
			engine->ClientCmd( bReverse ? "spec_prev" : "spec_next" );
		}
	}
	else if ( FStrEq( "show_freezepanel", szEventName ) )
	{
		PopulateDamageInfoEvent( evt );
	}
}

void CCSGO_HudSpecPlayer::ShowSpectatorPanel( bool bShow )
{
	static CPanoramaSymbol k_symSpectateVisible( "HudSpecplayerRoot--visible" );

	if ( bShow && !BHasClass(k_symSpectateVisible) )
	{
		AddClass( k_symSpectateVisible );
		m_nTargetID = -1;
	}
	else if ( !bShow )
	{
		if ( BHasClass(k_symSpectateVisible) )
			RemoveClass( k_symSpectateVisible );
	}

}

int CCSGO_HudSpecPlayer::NeedReplayUI()
{
	return g_HltvReplaySystem.GetHltvReplayDelay() != 0 && !CL_HltvReplayOnDeath();
}

void CCSGO_HudSpecPlayer::UpdateSpectatedPlayer( void )
{
	C_CSPlayer *pSpectatorTarget = ToCSPlayer( UTIL_PlayerByIndex( GetSpectatorTarget() ) );

	if ( m_bCallShowReplayUI )
	{
		ShowReplayUi();
		//ShowReplaySkull();
		m_bCallShowReplayUI = false;
	}

	C_CS_PlayerResource *pCSPR = GetCSResources();
	if ( !pCSPR )
		return;

	CCSGO_HudFreezePanel *pPanel = GET_HUDELEMENT( CCSGO_HudFreezePanel );
	if ( !pPanel )
		return;

	bool bShowPanel = ( cl_draw_only_deathnotices.GetBool() == false ) && ( cl_drawhud.GetBool() == true ) && ( pPanel->IsVisible() == false );

	if ( ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE ) && pSpectatorTarget && bShowPanel )
	{
		UpdateHotkeyText();
		UpdatePlayerStats();

		int nTeam = pSpectatorTarget->GetTeamNumber();
		m_nTargetID = GetSpectatorTarget();

		ShowSpectatorPanel( true );

		bool bDoingOverwatch = false;

		if ( CDemoPlaybackParameters_t const *pPlayback = engine->GetDemoPlaybackParameters() )
		{
			bDoingOverwatch = pPlayback->m_bAnonymousPlayerIdentity;
		}

		//static const CPanoramaSymbol k_symDynamicAvatar( "DynamicAvatar" );
		//static const CPanoramaSymbol k_symDefaultAvatarCT( "DefaultAvatarCT" );
		//static const CPanoramaSymbol k_symDefaultAvatarT( "DefaultAvatarT" );

		if ( m_nTargetID != m_iLastTargetID )
		{
			static const panorama::CPanoramaSymbol k_symHidden( "HudSpecplayer__Stats_DamageTaken--Hidden" );

			m_pDamageTakenPanel->SetHasClass( k_symHidden, !NeedReplayUI() || m_nDamageTaken == 0 );
			m_pDamageGivenPanel->SetHasClass( k_symHidden, !NeedReplayUI() || m_nDamageGiven == 0 );

			{
				wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
				pCSPR->GetDecoratedPlayerName( m_nTargetID, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot | k_EDecoratedPlayerNameFlag_Simple );
				char szPlayerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
				V_UnicodeToUTF8( wszPlayerName, szPlayerNameUTF8, ARRAYSIZE( szPlayerNameUTF8 ) );
				SetDialogVariable( "spec_player_name", szPlayerNameUTF8 );
			}
		
// 			CSteamID steamID;
// 			bool bShowDynamicAvatar = ( pCSPR->IsFakePlayer( m_nTargetID ) == false && pSpectatorTarget->GetSteamID( &steamID ) );
// 			if( bDoingOverwatch == false && bShowDynamicAvatar )
// 			{
// 				if ( m_pPlayerAvatar.Get() )
// 				{
// 					m_pPlayerAvatar->SetSteamID( steamID );
// 					m_pPlayerAvatar->SetVisible(true);
// 				}
// 			}
// 			else
// 			{
// 				if ( m_pPlayerAvatar.Get() )
// 				{
// 					m_pPlayerAvatar->SetVisible(false);
// 				}
// 
// 				if ( m_pPlayerAvatarDefault )
// 				{
// 					if ( nTeam == TEAM_CT )
// 					{
// 						m_pPlayerAvatarDefault->SetImage( "file://{images}/hud/teamcounter/teamcounter_alivebgCT.png" );
// 					}
// 					else if ( nTeam == TEAM_TERRORIST )
// 					{
// 						m_pPlayerAvatarDefault->SetImage( "file://{images}/hud/teamcounter/teamcounter_alivebgT.png" );
// 					}
// 				}
// 			}

			// Add color to avatar panel
			if ( CPanel2D* pAvatarColor = m_pPlayerAvatarColor.Get() )
			{
				bool bShowColor = false;
				Color playerColor;
				C_CSPlayer* pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

				if ( pLocalPlayer )
				{
					if ( CSGameRules() && CSGameRules()->GetSurvivalRules() && pSpectatorTarget->m_nSurvivalTeam >= 0 )
					{
						if ( pLocalPlayer->IsHLTV() || ( g_HltvReplaySystem.IsInPermanentReplay() && !g_HltvReplaySystem.GetHltvReplayDelay() ) )
						{
							// survival team colors for each team
							bShowColor = true;
							//playerColor = CSGameRules()->GetSurvivalRules()->GetTeamColor( pSpectatorTarget->m_nSurvivalTeam );
							playerColor = CSGameRules()->GetSurvivalRules()->GetTeamColorByUserID( pSpectatorTarget->GetUserID() );
						}
					}
					else if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( pCSPR->GetTeam( m_nTargetID ) ) )
					{
						int nColorID = pCSPR->GetCompTeammateColor( m_nTargetID );
						if ( nColorID >= 0 )
						{
							// competitive player colors on this team
							nColorID = nColorID % 5;
							playerColor = pCSPR->GetCompPlayerColorByID( nColorID );
							bShowColor = true;
						}
					}
				}

				static const CPanoramaSymbol k_symShowColor( "HudSpecplayerRoot__playercolor--show" );
				pAvatarColor->SetHasClass( k_symShowColor, bShowColor );
				if ( bShowColor && m_colorForAvatarPlayer != playerColor )
				{
					m_colorForAvatarPlayer = playerColor;
					panorama::IUIPanelStyle *pPanelStyle = pAvatarColor->AccessStyle();
					pPanelStyle->SetSimpleWashColor( playerColor );
				}
			}

			static const panorama::CPanoramaSymbol k_symTeamCT( "HudSpecplayerRoot--TeamCT" );
			static const panorama::CPanoramaSymbol k_symTeamT( "HudSpecplayerRoot--TeamT" );
			static const panorama::CPanoramaSymbol k_symTeamNone( "HudSpecplayerRoot--TeamNone" );
			static const panorama::CPanoramaSymbol k_classesToRemove[] = { k_symTeamCT, k_symTeamT, k_symTeamNone };
			RemoveClasses( k_classesToRemove, V_ARRAYSIZE( k_classesToRemove ) );

			extern ConVar spec_dz_group_teams;
			auto pSurvivalRules = CSGameRules()->GetSurvivalRules();
			if ( m_pColorStrip && spec_dz_group_teams.GetBool() && pSurvivalRules && pSurvivalRules->IsPlayingTeamMode() )
			{
				AddClass( k_symTeamNone );

				Color playerColor = CSGameRules()->GetSurvivalRules()->GetTeamColorByUserID( pSpectatorTarget->GetUserID() );
				panorama::IUIPanelStyle *pPanelStyle;
				pPanelStyle = m_pColorStrip->AccessStyle();
				pPanelStyle->SetSimpleWashColor( playerColor, true );
			}
			else
			{
				if ( nTeam == TEAM_CT )
				{
					AddClass( k_symTeamCT );
				}
				else if ( nTeam == TEAM_TERRORIST )
				{
					AddClass( k_symTeamT );
				}
				else
				{
					AddClass( k_symTeamNone );
				}
			}

			// get the team
			C_Team *pTeam = GetGlobalTeam( pSpectatorTarget->GetTeamNumber() );
			if ( pTeam && /*!uiClanID &&*/ !StringIsEmpty( pTeam->Get_LogoImageString() ) )
			{
				m_pDefaultTeamLogo->SetVisible(false);
				m_pTeamLogo->SetVisible(true);

				CUtlVector< CBackgroundImageLayer * > *pvecLayers = m_pTeamLogo->AccessStyle()->GetBackgroundImages();
				if ( pvecLayers )
				{
					FOR_EACH_VEC( *pvecLayers, i )
					{
						CBackgroundImageLayer *pLayer = pvecLayers->Element( i );
						char szImageFileName[_MAX_PATH];
						Q_snprintf( szImageFileName, sizeof( szImageFileName ), "materials/panorama/images/tournaments/teams/%s.svg", pTeam->Get_LogoImageString() );
						if ( g_pFullFileSystem && g_pFullFileSystem->FileExists( szImageFileName, "GAME" ) )
						{
							CUtlString strImagePath;
							strImagePath.Format( "file://{images}/tournaments/teams/%s.svg", pTeam->Get_LogoImageString() );
							pLayer->UnloadImage();
							pLayer->SetPath( strImagePath );
							pLayer->ReloadImage( m_pTeamLogo->UIPanel() );
						}
					}
				}
			}
			else
			{
				m_pDefaultTeamLogo->SetVisible( true );
				m_pTeamLogo->SetVisible( false );
			}

			m_iLastTargetID = m_nTargetID;
			m_iLastTargetControlledByID = pCSPR->GetControlledByPlayer( m_nTargetID );
			m_fNextUpdateTime = -1; // make sure we update the navigation bar now
		}

		C_WeaponCSBase *pWeapon = assert_cast< C_WeaponCSBase* >( pSpectatorTarget->GetActiveWeapon() );
		const CEconItemView *pItem = pWeapon ? pWeapon->GetEconItemView() : nullptr;
		if ( pWeapon && pItem && pItem->IsValid() && pItem->GetItemDefinition() )
		{			
			itemid_t unNewItemID = pItem->GetItemID() ? pItem->GetItemID() : pItem->GetFauxItemIDFromDefinitionIndex();
			if ( m_iLastItemId != unNewItemID )
			{
				bool bShowWeapon = false;
				if ( bShowWeapon && pItem->HasGeneratedInventoryImage() )
				{
					m_pWeaponImageContainer->SetVisible( true );
//					m_pWeaponImagePanel->SetItemID( unNewItemID );
				}
				else
				{
					m_pWeaponImageContainer->SetVisible( false );
				}

				// show the weapon text
// 				if ( pSpectatorTarget->IsAlive() && (pItem->HasGeneratedInventoryImage() || pWeapon->GetOriginalOwnerXuid() != g_PR->GetXuid( pSpectatorTarget->entindex() )) )
// 				{
// 					m_pWeaponNameLabel->SetVisible(true);
// 
// 					// NAME
// 					const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );
// 
// 					enum { kColorBufSize = 128 };
// 					wchar_t rwchColor[kColorBufSize];
// 					Q_UTF8ToUnicode( GetHexColorForAttribColor( pRarity->GetAttribColor() ), rwchColor, kColorBufSize );
// 
// 					wchar_t wszWeaponNameHTML[MAX_ITEM_CUSTOM_NAME_LENGTH * 8 + 64];
// 
// 					// HTML-escape name
// 					// $$$REI TODO: Use loc tag + dialog variables or something, it's pretty craptastic to be assigning this giant html to a label directly
// 					const wchar_t* wszItemName = pItem->GetItemName();
// 					wchar_t wszItemNameHTMLEncoded[MAX_ITEM_CUSTOM_NAME_LENGTH * 8];
// 					bool bEncodeCompleted = V_BasicHtmlEntityEncode( wszItemNameHTMLEncoded, V_ARRAYSIZE( wszItemNameHTMLEncoded ), wszItemName, V_wcslen( wszItemName ) );
// 					Assert( bEncodeCompleted );
// 
// 					V_swprintf_safe( wszWeaponNameHTML, L"<font color=\"" PRI_WS_FOR_WS L"\">" PRI_WS_FOR_WS L"</font>", rwchColor, wszItemNameHTMLEncoded );
// 
// 					player_info_t pi;
// 					engine->GetPlayerInfo( pSpectatorTarget->entindex(), &pi );
// 					//Assert( pi.xuid == CurrentOwnerXUID.xuid );
// 
// 					wchar_t wcTargetWeaponPossessed[128] = { 0 };
// 
// 					if ( ( pWeapon ) &&
// 						( pWeapon->GetOriginalOwnerXuid() != 0 ) &&
// 						( pWeapon->GetOriginalOwnerXuid() != pi.xuid ) )
// 					{
// 						wchar_t const * wszPlayerName;
// 
// 						wszPlayerName = CUiComponent_FriendsList::GetInstance()->GetPlayerName( pWeapon->GetOriginalOwnerXuid() );
// 
// 						g_pVGuiLocalize->ConstructString( wcTargetWeaponPossessed, sizeof( wcTargetWeaponPossessed ), g_pVGuiLocalize->Find( "CSGO_Weapon_Possessive" ), 2, wszPlayerName, wszWeaponNameHTML );
// 					}
//
// 					char szWeaponHTMLUTF8[MAX_ITEM_CUSTOM_NAME_LENGTH * 8 + 64];
// 					if ( StringIsEmpty( wcTargetWeaponPossessed ) )
// 						V_UnicodeToUTF8( wszWeaponNameHTML, szWeaponHTMLUTF8, ARRAYSIZE( szWeaponHTMLUTF8 ) );
// 					else
// 						V_UnicodeToUTF8( wcTargetWeaponPossessed, szWeaponHTMLUTF8, ARRAYSIZE( szWeaponHTMLUTF8 ) );
// 
// 					// set the panel here
// 					m_pWeaponNameLabel->SetDialogVariable( "weapon_description", szWeaponHTMLUTF8 );
// 				}
// 				else
				{
					m_pWeaponNameLabel->SetVisible(false);
				}

				m_iLastItemId = unNewItemID;
			}
		}
		else
		{
			m_pWeaponImageContainer->SetVisible( false );
			m_pWeaponNameLabel->SetVisible(false);
		}

// 		XUID xuid = g_PR->GetXuid( pSpectatorTarget->entindex() );
// 		extern const CEconItemView * Helper_FlairItem_GetFlairIDForPlayer( XUID xuid, itemid_t *pullFauxItemID = NULL );
// 		itemid_t ullDisplayFlairItemID = 0;
// 		const CEconItemView *pCoin = Helper_FlairItem_GetFlairIDForPlayer( xuid, &ullDisplayFlairItemID );
// 		if ( pCoin && pCoin->IsValid() && !bDoingOverwatch )
// 		{
// 			if ( m_iLastDisplayItemId != ullDisplayFlairItemID )
// 			{
// 				//m_pPlayerAvatarDisplayItem->SetImage( CFmtStr( "file://{images}/%s_small.png", pCoin->GetInventoryImage() ) );
// 				m_pPlayerAvatarDisplayItem->SetItemID( ullDisplayFlairItemID );
// 				m_iLastDisplayItemId = ullDisplayFlairItemID;
// 			}
// 
// 			m_pPlayerAvatarDisplayItem->SetVisible( true );
// 		}
// 		else
// 		{
// 			m_pPlayerAvatarDisplayItem->SetVisible( false );
// 		}
	}
	else
	{
		ShowSpectatorPanel( false );
	}
}

void CS_SpecPlayerGUI_OnTimeJump()
{
	CCSGO_HudSpecPlayer *pPanel = GET_HUDELEMENT( CCSGO_HudSpecPlayer );
	if ( pPanel )
		pPanel->OnTimeJump();
}

void CCSGO_HudSpecPlayer::OnTimeJump()
{
	m_fNextUpdateTime = -1;
	//m_bShowRequest = true;
	m_bCallShowReplayUI = true;
}

void CS_SpecPlayerGUI_OnRender2DEffectsPostHUD()
{
	CCSGO_HudSpecPlayer *pPanel = GET_HUDELEMENT( CCSGO_HudSpecPlayer );
	if ( pPanel )
		pPanel->UpdateReplayUI();
}

void CCSGO_HudSpecPlayer::UpdateReplayUI()
{
	C_CSPlayer *pPlayer = NULL;
	if ( NeedReplayUI() )
	{
		pPlayer = ToCSPlayer( UTIL_PlayerByIndex( g_HltvReplaySystem.GetPrimaryVictimEntIndex() ) );
	}
	else if ( g_HltvReplaySystem.IsDemoPlayback() )
	{
		if ( g_HltvReplaySystem.IsDemoPlaybackLowLights() )
			pPlayer = static_cast< C_CSPlayer* >( g_HltvReplaySystem.GetDemoPlaybackPlayer() );
	}
	UpdateReplayUI( pPlayer );
}

void CCSGO_HudSpecPlayer::ShowReplayUi()
{
	//WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
	//{
	//	m_pScaleformUI->ValueArray_SetElement( args, 0, NeedReplayUI() );
	//	m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowReplayUi", args, 1 );
	//}
}

class CHeadBoneLookupCache
{
	CStudioHdr *m_pStudioHdr;
	int m_nHeadBoneIndex;
	public:
	CHeadBoneLookupCache()
	{
		m_pStudioHdr = NULL;
		m_nHeadBoneIndex = -1;
	}
	int LookupHeadBone( C_BaseAnimating *pEntity )
	{
		CStudioHdr *pStudioHdr = pEntity->GetModelPtr();
		if ( pStudioHdr == m_pStudioHdr )
		{
			return m_nHeadBoneIndex;
		}
		if ( pStudioHdr )
		{
			m_pStudioHdr = pStudioHdr;
			m_nHeadBoneIndex = Studio_BoneIndexByName( pStudioHdr, "head_0" );
			if ( m_nHeadBoneIndex < 0 )
			{
				m_nHeadBoneIndex = Studio_BoneIndexByName( pStudioHdr, "ValveBiped.Bip01_Head" );
			}
			return m_nHeadBoneIndex;
		}
		else
		{
			return -1;
		}
	}
};
static CHeadBoneLookupCache s_PlayerBoneLookupCache;

void CCSGO_HudSpecPlayer::UpdateReplayUI( C_CSPlayer * pPlayer )
{
	if ( !m_pReplayPlayerTag )
		return;

	if ( !pPlayer )
	{
		ShowReplayPlayerTag( false );
		return;
	}

	MDLCACHE_CRITICAL_SECTION();
	Vector vecPlayerIconPosWorld = pPlayer->GetAbsOrigin();

	int nHeadBoneIndex = s_PlayerBoneLookupCache.LookupHeadBone( pPlayer );
	if ( nHeadBoneIndex > 0 )
	{
		if ( CBaseEntity *pRagdollEntity = pPlayer->m_hRagdoll.Get() )
		{
			C_CSRagdoll *pRagdoll = static_cast< C_CSRagdoll* >( pRagdollEntity );
			pRagdoll->GetBonePosition( nHeadBoneIndex, vecPlayerIconPosWorld );
		}
		else
		{
			pPlayer->GetBonePosition( nHeadBoneIndex, vecPlayerIconPosWorld );
		}
		vecPlayerIconPosWorld.z += 10;
	}
	else
	{
		vecPlayerIconPosWorld.z += ( ( pPlayer->GetFlags() & FL_DUCKING ) ? 50 : 70 );
	}

	Vector vecPlayerIconPosScreen;
	if ( ScreenTransform( vecPlayerIconPosWorld, vecPlayerIconPosScreen ) )
	{
		// behind
		ShowReplayPlayerTag( false );
	}
	else
	{
		extern void ConvertNormalizedScreenSpaceToPixelScreenSpace( Vector &in );
		ConvertNormalizedScreenSpaceToPixelScreenSpace( vecPlayerIconPosScreen );

		/*
		ScaleformDisplayInfo dinfo;
		dinfo.SetX( vecPlayerIconPosScreen.x );
		dinfo.SetY( vecPlayerIconPosScreen.y );
		dinfo.SetVisibility( true );
		m_pScaleformUI->Value_SetDisplayInfo( m_pReplayPlayerTag, &dinfo );
		*/

		int nScreenspaceX, nScreenspaceY;
		GetVectorInScreenSpace( vecPlayerIconPosWorld, nScreenspaceX, nScreenspaceY );

		// Convert to panorama space (ie 1920x1080)
		float flScaleFactor = 1080.0f / ( float )ScreenHeight();
		float flPanoramaX, flPanoramaY;
		flPanoramaX = nScreenspaceX * flScaleFactor;
		flPanoramaY = nScreenspaceY * flScaleFactor;

		CUtlVector<panorama::CTransform3D *> vecTransforms;

		vecTransforms.AddToTail( new panorama::CTransformTranslate3D( flPanoramaX, flPanoramaY, 0.0f ) );
		m_pReplayPlayerTag->SetTransform3DSimple( vecTransforms );

		ShowReplayPlayerTag( true );

		m_bReplayPlayerTagVisible = true; // override visibility
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudSpecPlayer::PopulateDamageInfoEvent( IGameEvent *pEvent )
{
	int iVictimIndex = pEvent->GetInt( "victim" );
	C_CSPlayer *pLocalPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );

	if ( !( pLocalPlayer && ( iVictimIndex == pLocalPlayer->entindex() ) ) )
	{
		return;
	}

	C_CS_PlayerResource *cs_PR = dynamic_cast< C_CS_PlayerResource * >( g_PR );
	if ( !cs_PR )
	{
		return;
	}

	// Get the entity who killed us
	int iKillerIndex = pEvent->GetInt( "killer" );
	CCSPlayer* pKiller = ToCSPlayer( ClientEntityList().GetBaseEntity( iKillerIndex ) );

	wchar_t wszkillerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
	wszkillerName[0] = '\0';

	int nHitsTaken = 0;
	int nDamTaken = 0;
	int nHitsGiven = 0;
	int nDamGiven = 0;

	if ( pKiller )
	{
		cs_PR->GetDecoratedPlayerName( iKillerIndex, wszkillerName, sizeof( wszkillerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );

		// Get damage info
		static ConVarRef sv_damage_print_enable( "sv_damage_print_enable" );
		if ( sv_damage_print_enable.GetBool() )
		{
			nHitsTaken = pEvent->GetInt( "hits_taken", 0 );
			nDamTaken = pEvent->GetInt( "damage_taken", 0 );
			nHitsGiven = pEvent->GetInt( "hits_given", 0 );
			nDamGiven = pEvent->GetInt( "damage_given", 0 );
		}
	}
	else
	{
		// No specific killer (falling suicide can cause this), so set killer info to victim info

		cs_PR->GetDecoratedPlayerName( iVictimIndex, wszkillerName, sizeof( wszkillerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );
	}

	char szKillerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
	V_UnicodeToUTF8( wszkillerName, szKillerNameUTF8, ARRAYSIZE( szKillerNameUTF8 ) );

	SetDialogVariable( "killer_name", szKillerNameUTF8 );

	PopulateDamageInfo( nHitsTaken, nDamTaken, nHitsGiven, nDamGiven );
}

//-----------------------------------------------------------------------------
// Purpose: Set nDamageTaken/nDamageGiven if you do not wish to display
//			the damage taken/given
//-----------------------------------------------------------------------------
void CCSGO_HudSpecPlayer::PopulateDamageInfo( int nHitsTaken, int nDamageTaken, int nHitsGiven, int nDamageGiven )
{
	static const panorama::CPanoramaSymbol k_symHidden( "HudSpecplayer__Stats_DamageTaken--Hidden" );

	m_nDamageGiven = nDamageGiven;
	m_nDamageTaken = nDamageTaken;

	m_pDamageTakenPanel->SetHasClass( k_symHidden, ( nDamageTaken == 0 ) );
	m_pDamageGivenPanel->SetHasClass( k_symHidden, ( nDamageGiven == 0 ) );

	SetDialogVariable( "hits_taken", nHitsTaken );
	SetDialogVariable( "damage_taken", nDamageTaken );
	SetDialogVariable( "hits_given", nHitsGiven );
	SetDialogVariable( "damage_given", nDamageGiven );

	m_pDamageTakenPanel->SetText( ( nHitsTaken > 1 ) ? "#Panorama_FreezePanel_DamageTaken_Multi" : "#Panorama_FreezePanel_DamageTaken" );
	m_pDamageGivenPanel->SetText( ( nHitsGiven > 1 ) ? "#Panorama_FreezePanel_DamageGiven_Multi" : "#Panorama_FreezePanel_DamageGiven" );
}

void CCSGO_HudSpecPlayer::ShowReplayPlayerTag( bool bShow )
{
	if ( m_bReplayPlayerTagVisible != bShow )
	{
		ShowReplayPlayerTag_Internal( bShow );
	}
}

void CCSGO_HudSpecPlayer::ShowReplayPlayerTag_Internal( bool bShow )
{
	if ( m_pReplayPlayerTag )
	{
		/*
		ScaleformDisplayInfo dinfo;
		dinfo.SetVisibility( bShow );
		m_pScaleformUI->Value_SetDisplayInfo( m_pReplayPlayerTag, &dinfo );
		*/
		m_pReplayPlayerTag->SetVisible(bShow);
		m_bReplayPlayerTagVisible = bShow;
	}
	else
	{
		m_bReplayPlayerTagVisible = false; // no player tag is visible: there's no player tag element
	}
}

void CCSGO_HudSpecPlayer::UpdatePlayerStats()
{
	/* removed for partner depot */
}

void CCSGO_HudSpecPlayer::UpdateHotkeyText()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
		return;

	double dblTimeNow = Plat_FloatTime();

	// fade out
	if ( dblTimeNow > (panorama::UIEngine()->GetLastInputTime() + SPECPANEL_HOTKEY_VISIBLETIME) )
	{
		if ( dblTimeNow > (panorama::UIEngine()->GetLastInputTime() + SPECPANEL_HOTKEY_VISIBLETIME + SPECPANEL_HOTKEY_FADEOUTTIME) )
		{
			//m_pHotKeyLabelContainer->SetVisible(false);
			m_pHotKeyLabelContainer->SetOpacity(0);
			return;
		}

		float flFrac = 1.0f - (dblTimeNow - (panorama::UIEngine()->GetLastInputTime() + SPECPANEL_HOTKEY_VISIBLETIME)) / SPECPANEL_HOTKEY_FADEOUTTIME;
		m_pHotKeyLabelContainer->SetOpacity(flFrac);
	}
	else
	{
		m_pHotKeyLabelContainer->SetOpacity(1.0f);
	}

	m_pHotKeyLabelContainer->SetVisible(true);

	// TODO DO THIS
	//int iObsMode =  pPlayer->GetObserverMode();

	//Update navigation text

	uint32_t nHotKeysFlag = 0;

	if ( cl_spec_show_bindings.GetBool() )
	{
		if ( g_HltvReplaySystem.IsHltvReplayButtonEnabled() && !g_HltvReplaySystem.IsDelayedReplayRequestPending() ) // no need to show "press F5 for replay" if we're about to show replay anyway
		{
			nHotKeysFlag |= ( 1 << HOTKEY_TYPE_REPLAYLASTKILL );
		}

		if ( pCSPR->IsFakePlayer( m_nTargetID ) )
		{
			if ( CanControlSpectatedTarget() )
			{
				nHotKeysFlag |= ( 1 << HOTKEY_TYPE_CONTROLBOT );
			}
		}

		if ( NeedReplayUI() )
		{
			static ConVarRef spec_replay_enable( "spec_replay_enable" );
			if ( spec_replay_enable.GetInt() == 2 )
			{
				; // no way to cancel replay in this mode, so don't show it
			}
			else
			{
				nHotKeysFlag |= ( 1 << HOTKEY_TYPE_CANCELREPLAY );
			}
		}
		else
		{
			nHotKeysFlag |= ( 1 << HOTKEY_TYPE_ARROWS );
			nHotKeysFlag |= ( 1 << HOTKEY_TYPE_MAP );

			if ( mp_forcecamera.GetInt() == OBS_ALLOW_ALL || ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR ) )
			{
				nHotKeysFlag |= ( 1 << HOTKEY_TYPE_CAMERA );
			}
		}

		if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
		{
			if ( pParameters->m_uiLockFirstPersonAccountID )
			{
				// No need to show "Next Player" hint since switching players is not allowed in locked mode
				nHotKeysFlag &= ~( 1 << HOTKEY_TYPE_CANCELREPLAY );
				nHotKeysFlag &= ~( 1 << HOTKEY_TYPE_ARROWS );
				// Cannot control camera in locked mode either
				nHotKeysFlag &= ~( 1 << HOTKEY_TYPE_CAMERA );
			}
		}

		if ( !NeedReplayUI() && CanToggleXRayView() && ( !pPlayer->IsHLTV() || ( pPlayer->IsHLTV() && spec_cameraman_xray.GetInt() == 0 ) ) )
		{
			nHotKeysFlag |= ( 1 << HOTKEY_TYPE_XRAY );
		}

		// If the autodirector is off, tell the user how to turn it on
		if ( pPlayer->IsHLTV() && !spec_autodirector.GetBool() )
		{
			if ( spec_autodirector_cameraman.GetInt() > 0 )
			{
				nHotKeysFlag |= ( 1 << HOTKEY_TYPE_CAMERAMANON );
			}
			else
			{
				nHotKeysFlag |= ( 1 << HOTKEY_TYPE_DIERCTORON );
			}
		}

		// Hide the 'autodirector on/off' hint in overwatch (since the overwatcher is locked into first person)		
		if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
		{
			if ( pParameters->m_uiLockFirstPersonAccountID )
			{
				nHotKeysFlag &= ~( 1 << HOTKEY_TYPE_CAMERAMANON );
				nHotKeysFlag &= ~( 1 << HOTKEY_TYPE_DIERCTORON );
			}
		}
	}

	static const panorama::CPanoramaSymbol k_symHidden( "HudSpecplayer__key-hints-text--hidden" );
	for ( int i = 0; i < HOTKEY_TYPE_MAX; ++i )
	{
		m_pHotKeys[i]->SetHasClass( k_symHidden, !( nHotKeysFlag & ( 1 << i ) ) );
	}
}

void CCSGO_HudSpecPlayer::DisplayEventServerImage( void )
{
	m_pServerSponserLogos->SetOpacity(0.0);

	// server_graphic 1 is for spectators only
// 	if ( Helper_GraphicEnabled() || cl_server_graphic1_enable.GetBool() )
// 	{
// 		if( m_pServerSponserLogos->IsSet() )
// 		{
// 			m_pServerSponserLogos->SetOpacity(1.0);
// 		}
// 	}
}

int CCSGO_HudSpecPlayer::FindNextSpectatorTarget( bool bReverse )
{
	// LEGACY: Ask teamcounter for next target
	if ( CCSGO_HudTeamCounter* pTC = dynamic_cast< CCSGO_HudTeamCounter* > ( GetHud().FindElement( "CCSGO_HudTeamCounter" ) ) )
	{
		return pTC->FindNextObserverTargetIndex( bReverse );
	}

	return -1;
}

void CCSGO_HudSpecPlayer::Think( void )
{
	UpdateSpectatedPlayer();
	
	if ( BIsVisible() )
	{
	}
	
}

void CCSGO_HudSpecPlayer::LevelInit( void )
{
// 	if ( cl_server_graphic1_enable.GetBool() || Helper_GraphicEnabled() )
// 	{
// 		Helper_SetServerGraphic( m_pServerSponserLogos, sv_server_graphic1.GetString() );
// 	}
}

