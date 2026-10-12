//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Freeze Panel visible on player death
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudfreezepanel.h"

#include "panorama/controls/label.h"
#include "csgo_hudwinpanel.h"
#include "panorama/csgo_avatarimage.h"

#include "hltvreplaysystem.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "viewrender.h"
//#include "uicomponents/uicomponent_friendslist.h"
#include "view.h"
//#include "econ/econ_holidays.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


extern float g_flFreezecamScreenshotFrameTimeStarted;
extern bool g_bFreezecamScreenshotFrameNumAssigned;
extern float g_flFreezeFlash[ MAX_SPLITSCREEN_PLAYERS ];
extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;
extern ConVar cl_freezecampanel_position_dynamic;
extern ConVar spec_freeze_panel_replay_position;

#define FREEZECAM_SCREENSHOT_STRING "got the upper hand!"


DECLARE_PANORAMA_EVENT1( CSGOHudFreezePanelSetFlair, const char * );
DEFINE_PANORAMA_EVENT_DOC( CSGOHudFreezePanelSetFlair, "xuid", "Display flair image and medal text for the given player xuid" );

REGISTER_PANEL2D_FACTORY( CCSGO_HudFreezePanel, CSGOHudFreezePanel );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudFreezePanel::CCSGO_HudFreezePanel( panorama::CPanel2D *pParent, const char *pchID )
:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudFreezePanel", this )
{
	SetHiddenBits( HIDEHUD_MISCSTATUS );

	RequireLoadLayout( "file://{resources}/layout/hud/hudfreezepanel.xml" );
	AddClass( "FreezePanelRoot" );		// Not getting read from the xml for some reason

	m_pFreezePanel = RequireChildInLayoutFile( "FreezePanel" );
	m_pAvatarPanel = panorama::panel_cast< CCSGO_AvatarImage* >( RequireChildInLayoutFile( "Avatar" ) );
	m_pAvatarDefaultTerroristPanel = RequireChildInLayoutFile( "AvatarDefaultT" );
	m_pAvatarDefaultCTPanel = RequireChildInLayoutFile( "AvatarDefaultCT" );
	m_pAvatarHealthPanel = panorama::panel_cast< panorama::CProgressBar* >( RequireChildInLayoutFile( "AvatarHealthBar" ) );
	m_pDescriptionTextPanel = panorama::panel_cast< panorama::CLabel* >( RequireChildInLayoutFile( "DescriptionText" ) );
	m_pDamageTakenPanel = panorama::panel_cast< panorama::CLabel* >( RequireChildInLayoutFile( "DamageTaken" ) );
	m_pDamageGivenPanel = panorama::panel_cast< panorama::CLabel* >( RequireChildInLayoutFile( "DamageGiven" ) );
	m_pItemContainerPanel = RequireChildInLayoutFile( "ItemContainer" );
	m_pWeaponImagePanel = panorama::panel_cast< panorama::CImagePanel* >( RequireChildInLayoutFile( "WeaponImage" ) );
	m_pNavigationCancelPanel = RequireChildInLayoutFile( "NavigationCancel" );
	m_pNavigationSnapshotPanel = RequireChildInLayoutFile( "NavigationSnapshot" );
	m_pNavigationReplayPanel = RequireChildInLayoutFile( "NavigationReplay" );

	m_pFreezePanelSS = RequireChildInLayoutFile( "FreezePanelSS" );
	m_pAvatarSSPanel = panorama::panel_cast<CCSGO_AvatarImage*>( RequireChildInLayoutFile( "AvatarSS" ) );
	m_pAvatarDefaultTerroristSSPanel = RequireChildInLayoutFile( "AvatarDefaultTSS" );
	m_pAvatarDefaultCTSSPanel = RequireChildInLayoutFile( "AvatarDefaultCTSS" );
	m_pAvatarHealthSSPanel = panorama::panel_cast<panorama::CProgressBar*>( RequireChildInLayoutFile( "AvatarHealthBarSS" ) );
	m_pDescriptionTextSSPanel = panorama::panel_cast< panorama::CLabel* >( RequireChildInLayoutFile( "DescriptionTextSS" ) );
	m_pItemContainerSSPanel = RequireChildInLayoutFile( "ItemContainerSS" );
	m_pWeaponImageSSPanel = panorama::panel_cast<panorama::CImagePanel*>( RequireChildInLayoutFile( "WeaponImageSS" ) );

	m_pFreezeCancelPanel = RequireChildInLayoutFile( "FreezeCancel" );
	m_pSurvivalEndOfMatchShow = RequireChildInLayoutFile( "SurvivalEndOfMatch" );

	RegisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_HudFreezePanel::OnStyleFileReloaded );

	GetLayoutDefines();

	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudFreezePanel::~CCSGO_HudFreezePanel()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::LevelInit( void )
{
	ResetData();
	
	// Listen for events
	ListenForGameEvent( "show_freezepanel" );
	ListenForGameEvent( "hide_freezepanel" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_spawn" );
	ListenForGameEvent( "hltv_replay" );

	ShowPanel( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::LevelShutdown( void )
{
	StopListeningForAllEvents();

	ResetData();

	ShowPanel( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::ProcessInput( void )
{
	PositionPanel();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudFreezePanel::ShouldDraw( void )
{
	return m_bIsVisible && cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::TakeFreezeShot( void )
{
	if ( !ShouldDraw() )
	{
		return;
	}

	// Establish the frame when we are starting freezecam shot
	g_bFreezecamScreenshotFrameNumAssigned = true;
	g_flFreezecamScreenshotFrameTimeStarted = gpGlobals->curtime;

	// Hide the main freeze panel and show the "screenshot" one
	ShowFreezePanel( false );
	ShowFreezePanelScreenshot( true );

	// Position and rotate panel

	static const panorama::CPanoramaSymbol k_symDefaultPos( "FreezePanelSS--DefaultPosition" );

	if ( cl_freezecampanel_position_dynamic.GetInt() > 0 )
	{
		m_pFreezePanelSS->RemoveClass( k_symDefaultPos );

		int nScreenspaceX = 0;
		int nScreenspaceY = 0;
		float flRot = 0.0f;
		C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );

		if ( pPlayer )
		{
			flRot = -pPlayer->GetFreezeFrameTilt();
		}

		nScreenspaceY = ScreenHeight() - ( ScreenHeight() * 0.3 );
		if ( flRot > 0 )
		{
			nScreenspaceY -= ( ScreenHeight() * 0.2 );
		}

		nScreenspaceX = -( ScreenWidth() * 0.05 );
		// widescreen
		if ( ScreenHeight() / ScreenWidth() < 0.75 )
		{
			nScreenspaceX = -( ScreenWidth() * 0.01 );
		}
		
		CUtlVector<panorama::CTransform3D *> vecTransforms;

		// Convert to panorama space (ie 1920x1080)
		float flScaleFactor = 1080.0f / (float)ScreenHeight();
		float flPanoramaX = nScreenspaceX * flScaleFactor;
		float flPanoramaY = nScreenspaceY * flScaleFactor;

		vecTransforms.AddToTail( new panorama::CTransformRotate3D( 0.0f, flRot, 0.0f ) );
		vecTransforms.AddToTail( new panorama::CTransformTranslate3D( flPanoramaX, flPanoramaY, 0.0f ) );
		m_pFreezePanelSS->SetTransform3D( vecTransforms );
	}
	else
	{
		m_pFreezePanelSS->AddClass( k_symDefaultPos );
	}

	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		//Do effects
		g_flFreezeFlash[ GetSplitScreenPlayerSlot() ] = gpGlobals->curtime + 0.75f;
		pPlayer->EmitSound( "FreezeShot.TakeScreenshot" );

		//Extend Freezecam by a couple more seconds.
		engine->ClientCmd( "extendfreeze" );
		view->FreezeFrame( 3.0f );

		m_bHoldingAfterScreenshot = true;

		/*
		// Hide everything?
		if ( hud_freezecamhide.GetBool() )
		{
		SetVisible( false );
		DeleteCalloutPanels();
		}
		*/

		//Set the screenshot name
		if ( m_iKillerIndex <= MAX_PLAYERS )
		{
			const char *pszKillerName = g_PR->GetPlayerName( m_iKillerIndex );

			if ( pszKillerName )
			{
				ConVarRef cl_screenshotname( "cl_screenshotname" );

				if ( cl_screenshotname.IsValid() )
				{
					char szScreenShotName[512];

					Q_snprintf( szScreenShotName, sizeof( szScreenShotName ), "%s %s", GetFilesafePlayerName( pszKillerName ), FREEZECAM_SCREENSHOT_STRING );

					cl_screenshotname.SetValue( szScreenShotName );
				}
			}

			// clear the navigation text (space to skip)
			// TODO ??? Don't think it is necessary as it is not part of the freeze panel (screenshot mode)
			// PopulateNavigationText();

			C_CSPlayer *pKiller = ToCSPlayer( UTIL_PlayerByIndex( m_iKillerIndex ) );
			if ( pKiller && !pKiller->IsBot() )
			{
				CSteamID steamID;
				if ( pKiller->GetSteamID( &steamID ) && steamID.IsValid() )
				{
					ConVarRef cl_screenshotusertag( "cl_screenshotusertag" );
					if ( cl_screenshotusertag.IsValid() )
					{
						cl_screenshotusertag.SetValue( (int)steamID.GetAccountID() );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim )
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::OnHltvReplayButtonStateChanged( void )
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::FireGameEvent( IGameEvent * event )
{
	const char *pEventName = event->GetName();

	if ( V_strcmp( "player_death", pEventName ) == 0 )
	{
		OnPlayerDeathGameEvent( event );
	}
	else if ( V_strcmp( "player_spawn", pEventName ) == 0 )
	{
		OnPlayerSpawnGameEvent( event );
	}
	else if ( V_strcmp( "hide_freezepanel", pEventName ) == 0 )
	{
		OnHideFreezePanelGameEvent( event );
	}
	else if ( V_strcmp( "show_freezepanel", pEventName ) == 0 )
	{
		OnShowFreezePanelGameEvent( event );
	}
	else if ( V_strcmp( "hltv_replay", pEventName ) == 0 )
	{
		if ( event->GetInt( "reason" ) == REPLAY_EVENT_VICTORY )
		{	// victory replays show no kill tile
			OnHideFreezePanelGameEvent( event );
		}
		else if ( !event->GetInt( "delay" ) )
		{	// when the HLTV replay ends hide the freeze panel tile
			OnHideFreezePanelGameEvent( event );
		}
		else
		{	// when the HLTV replay starts in survival mode show a way to show stats
			bool bPlayingSurvival = CSGameRules() && CSGameRules()->IsPlayingSurvival();
			if ( bPlayingSurvival )
				ShowCancelPanel( k_ECancelPanelType_Survival );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::OnPlayerDeathGameEvent( IGameEvent *pEvent )
{
	m_bDominationIconVisible = false;

	// see if the local player died
	int iPlayerIndexVictim = engine->GetPlayerForUserID( pEvent->GetInt( "userid" ) );
	int iPlayerIndexKiller = engine->GetPlayerForUserID( pEvent->GetInt( "attacker" ) );

	//SF_SPLITSCREEN_PLAYER_GUARD();
	C_BasePlayer *pVictimPlayer = UTIL_PlayerByIndex( iPlayerIndexVictim ), *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	CCSPlayer* pKiller = ToCSPlayer( ClientEntityList().GetBaseEntity( iPlayerIndexKiller ) );
	m_FollowEntity = pKiller;

	const CEconItemView *pItem = nullptr;

	extern ConVar spec_replay_others_experimental;
	if ( !g_HltvReplaySystem.GetHltvReplayDelay() && ( spec_replay_others_experimental.GetBool() || ( pLocalPlayer && iPlayerIndexVictim == pLocalPlayer->entindex() ) ) )
	{
		// we need to notify the replay system that the replay is available before some other calls that happen  on this very event, so it would not be reliable to just have replay system listen to player_death event
		m_bHoldingAfterScreenshot = false;

		bool bShowItem = false;
		const char * szWeapon = pEvent->GetString( "weapon" );
		wchar_t wszWeaponNameHTML[MAX_ITEM_CUSTOM_NAME_LENGTH * 8 + 64];
		const char *pszLocString = nullptr;
		wchar_t const * wszOtherPlayerName = nullptr;

		if ( szWeapon && szWeapon[0] != 0 )
		{
			//get weapon name and custom name for the weapon info panel

			const char* szEventWeaponItemID = pEvent->GetString( "weapon_itemid" );
			uint64 itemid = Q_atoui64( szEventWeaponItemID );

			const char* szEventWeaponFauxItemID = pEvent->GetString( "weapon_fauxitemid" );
			uint64 fauxitemid = Q_atoui64( szEventWeaponFauxItemID );

			const char* szEventWeaponOriginalOwnerXuid = pEvent->GetString( "weapon_originalowner_xuid" );
			uint64 ullWeaponOriginalOwnerXuid = Q_atoui64( szEventWeaponOriginalOwnerXuid );

			if ( pKiller )
			{
				if ( !StringIsEmpty( szEventWeaponItemID ) )
				{
					// Get the real item if it's still in in the SO cache,
					pItem = CEconItemView::FindOrCreateEconItemViewForItemID( itemid );

					// if we don't have the item, see if the killer still has the item equiped and use that instead
					if ( !pItem && pKiller->GetActiveCSWeapon() && pKiller->GetActiveCSWeapon()->GetEconItemView() && pKiller->GetActiveCSWeapon()->GetEconItemView()->GetItemID() == itemid )
					{
						pItem = pKiller->GetActiveCSWeapon()->GetEconItemView();
					}

					//  otherwise fallback to the faux version
					if ( !pItem )
					{
						pItem = CEconItemView::FindOrCreateEconItemViewForItemID( fauxitemid );
					}
				}

				if ( pItem && pItem->IsValid() )
				{
					const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );

					enum { kColorBufSize = 128 };
					wchar_t rwchColor[kColorBufSize];
					Q_UTF8ToUnicode( GetHexColorForAttribColor( pRarity->GetAttribColor() ), rwchColor, kColorBufSize );

					// HTML-escape name
					// $$$REI TODO: Use loc tag + dialog variables or something, it's pretty craptastic to be assigning this giant html to a label directly
					const wchar_t* wszItemName = pItem->GetItemName();
					wchar_t wszItemNameHTMLEncoded[MAX_ITEM_CUSTOM_NAME_LENGTH * 8];
					bool bEncodeCompleted = V_BasicHtmlEntityEncode( wszItemNameHTMLEncoded, V_ARRAYSIZE( wszItemNameHTMLEncoded ), wszItemName, V_wcslen( wszItemName ) );
					Assert( bEncodeCompleted );

					V_swprintf_safe( wszWeaponNameHTML, L"<font color=\"" PRI_WS_FOR_WS L"\">" PRI_WS_FOR_WS L"</font>", rwchColor, wszItemNameHTMLEncoded );

					bShowItem = ( pItem->GetItemID() != 0 );
				}
				else	// we have no econitemview. Print generic message.
				{
					if ( StringHasPrefixCaseSensitive( szWeapon, "prop_exploding_barrel" ) )	//"prop_exploding_barrel"
					{
						V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Exploding_Barrel" ) );
					}
					else
					{
						if ( StringHasPrefixCaseSensitive( szWeapon, "hegrenade" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_HE_Grenade" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "frag_grenade" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_frag_Grenade" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "flashbang" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Flashbang" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "diversion" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Diversion" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "decoy" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Decoy" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "smokegrenade" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Smoke_Grenade" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "firebomb" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Firebomb" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "incgrenade" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_IncGrenade" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "molotov" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Molotov" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "breachcharge" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_BreachCharge" ) );
						}
						else if ( StringHasPrefixCaseSensitive( szWeapon, "planted_c4_survival" ) )
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#CSGO_Type_C4" ) );
						}
						else if ( !V_strcmp( szWeapon, "inferno" ) )
						{
							pszLocString = "#Panorama_FreezePanel_Killer1_Weapon_Plural";
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_Inferno" ) );
						}
						else
						{
							g_pVGuiLocalize->ConvertANSIToUnicode( szWeapon, wszWeaponNameHTML, sizeof( wszWeaponNameHTML ) );
						}
					}

					bShowItem = false;
				}

				CSteamID pKillerID;
				pKiller->GetSteamID( &pKillerID );
				CSteamID pVictimID = steamapicontext->SteamUser() ? steamapicontext->SteamUser()->GetSteamID() : CSteamID();

				if ( !pszLocString )
				{
					if ( pVictimID.IsValid() && pVictimID.ConvertToUint64() == ullWeaponOriginalOwnerXuid )
					{
						pszLocString = "#Panorama_FreezePanel_Killer1_YourWeapon"; // Victim's weapon
					}
					else if ( pKillerID.ConvertToUint64() == ullWeaponOriginalOwnerXuid ||
						( ( ullWeaponOriginalOwnerXuid == 0 ) && pKiller->IsBot() ) )
					{
						pszLocString = "#Panorama_FreezePanel_Killer1_KillerWeapon"; // Killer's weapon
					}
					else if ( ullWeaponOriginalOwnerXuid != 0 )
					{
						pszLocString = "#Panorama_FreezePanel_Killer1_OthersWeapon"; // Someone else's weapon
					}
					else
					{
						pszLocString = "#Panorama_FreezePanel_Killer1_Weapon";
					}
				}
			}
		}

		// Check if modes support dominations
		bool bDominationSupported = true;
		if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
			bDominationSupported = false;

		// the local player is dead, see if this is a new nemesis or a revenge
		if ( bDominationSupported && ( pEvent->GetInt( "dominated" ) > 0 ) )
		{
			PopulateDominationInfo( Nemesis, "#FreezePanel_NewNemesis1", nullptr, nullptr );
		}
		// was the killer your pre-existing nemesis?
		else if ( bDominationSupported && pKiller && pKiller->IsPlayerDominated( iPlayerIndexVictim ) )
		{
			PopulateDominationInfo( Nemesis, "#FreezePanel_OldNemesis1", nullptr, nullptr );
		}
		else if ( pEvent->GetInt( "revenge" ) > 0 )
		{
			PopulateDominationInfo( Revenge, "#FreezePanel_Revenge1", nullptr, nullptr );
		}
		else if ( pKiller == pVictimPlayer || pKiller == nullptr )
		{
			// A few special suicide explanations
			if ( !V_strcmp( szWeapon, "dangerzone_controller" ) )
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledByDangerZone", nullptr, nullptr );
			}
			else if ( !V_strcmp( szWeapon, "env_gunfire" ) )
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledByAutoSentry", nullptr, nullptr );
			}
			else if ( StringHasPrefixCaseSensitive( szWeapon, "prop_exploding_barrel" ) )	//"prop_exploding_barrel"
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledByExplodingBarrel", nullptr, nullptr );
			}
			else if ( StringHasPrefixCaseSensitive( szWeapon, "hegrenade" )
				|| StringHasPrefixCaseSensitive( szWeapon, "flashbang" )
				|| StringHasPrefixCaseSensitive( szWeapon, "decoy" )
				|| StringHasPrefixCaseSensitive( szWeapon, "smokegrenade" ) )
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledByOwnGrenade", nullptr, nullptr );
			}
			else if ( StringHasPrefixCaseSensitive( szWeapon, "incgrenade" )
				|| StringHasPrefixCaseSensitive( szWeapon, "molotov" )
				|| !V_strcmp( szWeapon, "inferno" ) )
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledByFire", nullptr, nullptr );
			}
			else if ( StringHasPrefixCaseSensitive( szWeapon, "breachcharge" ) )
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledByOwnBreachCharge", nullptr, nullptr );
			}
			else if ( StringHasPrefixCaseSensitive( szWeapon, "planted_c4_survival" ) )
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledByOwnC4", nullptr, nullptr );
			}
			else
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledSelf", nullptr, nullptr );
			}
		}
		else
		{
			PopulateDominationInfo( None, pszLocString, wszWeaponNameHTML, wszOtherPlayerName );
		}

		// TODO: this is not the correct thing to do!  This assumes that the player's active weapon is the weapon that killed the other player, this is not always true!!!
		// this set the item panel
		static const panorama::CPanoramaSymbol k_symItemContainerHidden( "FreezePanel__ItemContainer--Hidden" );
		m_pItemContainerPanel->SetHasClass( k_symItemContainerHidden, !bShowItem );
		m_pItemContainerSSPanel->SetHasClass( k_symItemContainerHidden, !bShowItem );
		if ( bShowItem && pItem && pItem->IsValid() )
		{
			CUtlString strDefaultImagePath;
			strDefaultImagePath.Format( "file://{images_econ}/%s.png", pItem->GetInventoryImage() );
			m_pWeaponImagePanel->SetImage( strDefaultImagePath.Get() );
			m_pWeaponImageSSPanel->SetImage( strDefaultImagePath.Get() );

			if ( pItem->HasGeneratedInventoryImage() )
			{
				panorama::CPanelPtr< panorama::CImagePanel > pImage( m_pWeaponImagePanel );
				panorama::CPanelPtr< panorama::CImagePanel > pImageSS( m_pWeaponImageSSPanel );
				const_cast<CEconItemView*>( pItem )->GenerateInventoryImage( [pImage, pImageSS]( const CEconItemView * pItem, CUtlBuffer &rawImageRgba, int nWidth, int nHeight, itemid_t unItemId ) -> void
				{
					if ( pImage.Get() )
					{
						pImage->SetImage( rawImageRgba, nWidth, nHeight, NULL, panorama::k_EImageFormatB8G8R8A8 );
					}
					if ( pImageSS.Get() )
					{
						pImageSS->SetImage( rawImageRgba, nWidth, nHeight, NULL, panorama::k_EImageFormatB8G8R8A8 );
					}
				} );
			}
		}

		PopulateNavigationText();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::OnPlayerSpawnGameEvent( IGameEvent *pEvent )
{
	// TODO
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::OnHideFreezePanelGameEvent( IGameEvent *pEvent )
{
	g_bFreezecamScreenshotFrameNumAssigned = false;
	m_bFreezePanelStateRelevant = false;

	ShowPanel( false );

	m_bHoldingAfterScreenshot = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::OnShowFreezePanelGameEvent( IGameEvent *pEvent )
{
	g_bFreezecamScreenshotFrameNumAssigned = true;
	g_flFreezecamScreenshotFrameTimeStarted = gpGlobals->curtime;

	int iVictimIndex = pEvent->GetInt( "victim" );
	C_CSPlayer *pLocalPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );

	if ( !( pLocalPlayer && ( iVictimIndex == pLocalPlayer->entindex() ) ) )
	{
		return;
	}

	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
	if ( !cs_PR )
	{
		return;
	}
	
	bool bPauseBeforeAutoreplay = g_HltvReplaySystem.IsDelayedReplayRequestPending();
	bool bPlayingSurvival = CSGameRules() && CSGameRules()->IsPlayingSurvival();
	ShowPanel( bPlayingSurvival || !bPauseBeforeAutoreplay );
	if ( bPlayingSurvival )
		ShowCancelPanel( k_ECancelPanelType_None );
	else
		ShowCancelPanel( bPauseBeforeAutoreplay ? k_ECancelPanelType_Replay : k_ECancelPanelType_None );
	m_bFreezePanelStateRelevant = true;

	// Get the entity who killed us
	m_iKillerIndex = pEvent->GetInt( "killer" );
	CCSPlayer* pKiller = ToCSPlayer( ClientEntityList().GetBaseEntity( m_iKillerIndex ) );
	m_FollowEntity = pKiller;

	wchar_t wszkillerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
	wszkillerName[0] = '\0';
	int nHitsTaken = 0;
	int nDamTaken = 0;
	int nHitsGiven = 0;
	int nDamGiven = 0;
	XUID xuidKiller = INVALID_XUID;
	int nKillerTeam = TEAM_INVALID;
	float flKillerHealth = -1.0f;	// value [0.0f, 1.0f], negative numbers to hide health bar

	if ( pKiller )
	{
		xuidKiller = g_PR->GetXuid( m_iKillerIndex );
		nKillerTeam = pKiller->GetTeamNumber();
		
		// Killer health
		int iKillerHealth = Max( pKiller->GetHealth(), 0 );
		flKillerHealth = ( (float)iKillerHealth / (float)pKiller->GetMaxHealth() );
		if ( mp_forcecamera.GetInt() != OBS_ALLOW_ALL )
		{
			// we don't want to show the killer's health unless player can spectate killer
			// -1 lets the script know what we should hide it
			flKillerHealth = -1.0f;
		}
		else if ( !pKiller->IsAlive() )
		{
			flKillerHealth = 0.0f;
		}

		cs_PR->GetDecoratedPlayerName( m_iKillerIndex, wszkillerName, sizeof( wszkillerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );

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

		xuidKiller = g_PR->GetXuid( iVictimIndex );
		nKillerTeam = pLocalPlayer->GetTeamNumber();
		cs_PR->GetDecoratedPlayerName( iVictimIndex, wszkillerName, sizeof( wszkillerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );
	}

	// Convert player's name to UTF-8 strings
	// Each Unicode code point can expand to as many as four bytes in UTF-8
	char szKillerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
	V_UnicodeToUTF8( wszkillerName, szKillerNameUTF8, ARRAYSIZE( szKillerNameUTF8 ) );

	SetDialogVariable( "killer_name", szKillerNameUTF8 );

	PopulateDamageInfo( nHitsTaken, nDamTaken, nHitsGiven, nDamGiven );

	PopulateAvatarInfo( CSteamID( xuidKiller ), nKillerTeam, flKillerHealth );

	PopulateFreezeFrameBorder( pLocalPlayer );

	m_nFreezePanelPosY = 0.0f;
	m_pFreezePanel->ClearPropertyFromCode( "transform" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::ResetData()
{
	m_FollowEntity = NULL;
	m_iKillerIndex = 0;
	m_nFreezePanelPosY = 0.0f;
	
	m_bIsVisible = false;
	m_bIsFreezePanelVisible = false;
	m_bHoldingAfterScreenshot = false;
	m_bFreezePanelStateRelevant = false;
	m_bDominationIconVisible = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::ShowPanel( bool bShow )
{
	static const panorama::CPanoramaSymbol k_symHidden( "FreezePanelRoot--Hidden" );

	SetHasClass( k_symHidden, !bShow );

	m_bIsVisible = bShow;

	ShowFreezePanel( bShow );
	ShowFreezePanelScreenshot( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::ShowFreezePanel( bool bShow )
{
	static const panorama::CPanoramaSymbol k_symHidden( "FreezePanel--Hidden" );
	static const panorama::CPanoramaSymbol k_symFadeIn( "FreezePanel--FadeIn" );

	m_pFreezePanel->SetHasClass( k_symHidden, !bShow );
	m_bIsFreezePanelVisible = bShow;

	if ( bShow )
	{
		m_pFreezePanel->TriggerClass( k_symFadeIn );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::ShowFreezePanelScreenshot( bool bShow )
{
	static const panorama::CPanoramaSymbol k_symHidden( "FreezePanelSS--Hidden" );
	m_pFreezePanelSS->SetHasClass( k_symHidden, !bShow );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::ShowCancelPanel( ECancelPanelType_t eType )
{
	static const panorama::CPanoramaSymbol k_symHidden( "FreezeCancel--Hidden" );
	m_pFreezeCancelPanel->SetHasClass( k_symHidden, eType != k_ECancelPanelType_Replay );
	m_pSurvivalEndOfMatchShow->SetHasClass( k_symHidden, eType != k_ECancelPanelType_Survival );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CCSGO_HudFreezePanel::GetFilesafePlayerName( const char *pszOldName )
{
	if ( !pszOldName )
		return "";

	static char szSafeName[MAX_PLAYER_NAME_LENGTH];
	int nSafeNameBufSize = sizeof( szSafeName );
	int nNewPos = 0;

	for ( const char *p = pszOldName; *p != 0 && nNewPos < nSafeNameBufSize - 1; p++ )
	{
		if ( *p == '.' )
		{
			szSafeName[nNewPos] = '-';
		}
		else if ( *p == '/' )
		{
			szSafeName[nNewPos] = '-';
		}
		else if ( *p == '\\' )
		{
			szSafeName[nNewPos] = '-';
		}
		else if ( *p == ':' )
		{
			szSafeName[nNewPos] = '-';
		}
		else
		{
			szSafeName[nNewPos] = *p;
		}

		nNewPos++;
	}

	szSafeName[nNewPos] = 0;

	return szSafeName;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::PopulateNavigationText()
{
	static const panorama::CPanoramaSymbol k_symHidden( "FreezePanel__Navigation--Hidden" );

	bool bCancelTextVisible = false;
	bool bSnapshotTextVisible = false;
	bool bReplayTextVisible = false;

	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		bCancelTextVisible = true;
	}
	else
	{
		bSnapshotTextVisible = true;
		if ( g_HltvReplaySystem.IsHltvReplayButtonEnabled() && !g_HltvReplaySystem.IsDelayedReplayRequestPending() ) // no need to show "press F5 for replay" if we're about to show replay anyway
		{
			bReplayTextVisible = true;
		}
	}

	m_pNavigationCancelPanel->SetHasClass( k_symHidden, !bCancelTextVisible );
	m_pNavigationSnapshotPanel->SetHasClass( k_symHidden, !bSnapshotTextVisible );
	m_pNavigationReplayPanel->SetHasClass( k_symHidden, !bReplayTextVisible );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::PopulateDominationInfo( DominationIconType iconType, const char* szlocalizationToken, const wchar_t *wszWeaponHTML, const wchar_t *wszOtherPlayerName )
{	
	if ( wszWeaponHTML && wszWeaponHTML[0] )
	{
		// Convert weapon's name to UTF-8 strings
		// Each Unicode code point can expand to as many as four bytes in UTF-8
		// However, we already encoded HTML-entities in this, so each character might
		// have encoded into a larger (ascii) html entity which we reserve 8 bytes for.
		char szWeaponHTMLUTF8[8 * MAX_ITEM_CUSTOM_NAME_LENGTH + 64];
		V_UnicodeToUTF8( wszWeaponHTML, szWeaponHTMLUTF8, ARRAYSIZE( szWeaponHTMLUTF8 ) );

		SetDialogVariable( "weapon_name", szWeaponHTMLUTF8 );
	}
	else
	{
		SetDialogVariable( "weapon_name", "" );
	}

	// Convert "wszOtherPlayerName" to UTF-8 string
	if ( wszOtherPlayerName && wszOtherPlayerName[0] )
	{
		char szOtherPlayerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
		V_UnicodeToUTF8( wszOtherPlayerName, szOtherPlayerNameUTF8, ARRAYSIZE( szOtherPlayerNameUTF8 ) );

		SetDialogVariable( "other_player_name", szOtherPlayerNameUTF8 );
	}
	else
	{
		SetDialogVariable( "other_player_name", "" );
	}

	m_pDescriptionTextPanel->SetText( szlocalizationToken );

	// Domination icons

	static const panorama::CPanoramaSymbol k_symDominationIconType( "DominationIconType" );
	static const panorama::CPanoramaSymbol k_symDominationIconTypeClasses[] = { "DominationIconTypeNone", "DominationIconTypeNemesis", "DominationIconTypeRevenge" };
	COMPILE_TIME_ASSERT( ARRAYSIZE( k_symDominationIconTypeClasses ) == DominationIconMax );
	
	m_pFreezePanel->SwitchClass( k_symDominationIconType, k_symDominationIconTypeClasses[iconType] );
}


//-----------------------------------------------------------------------------
// Purpose: Set nDamageTaken/nDamageGiven if you do not wish to display
//			the damage taken/given
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::PopulateDamageInfo( int nHitsTaken, int nDamageTaken, int nHitsGiven, int nDamageGiven )
{
	static const panorama::CPanoramaSymbol k_symHidden( "FreezePanel__Damage--Hidden" );

	m_pDamageTakenPanel->SetHasClass( k_symHidden, ( nDamageTaken == 0 ) );
	m_pDamageGivenPanel->SetHasClass( k_symHidden, ( nDamageGiven == 0 ) );

	SetDialogVariable( "hits_taken", nHitsTaken );
	SetDialogVariable( "damage_taken", nDamageTaken );
	SetDialogVariable( "hits_given", nHitsGiven );
	SetDialogVariable( "damage_given", nDamageGiven );

	m_pDamageTakenPanel->SetText( ( nHitsTaken > 1 ) ? "#Panorama_FreezePanel_DamageTaken_Multi" : "#Panorama_FreezePanel_DamageTaken" );
	m_pDamageGivenPanel->SetText( ( nHitsGiven > 1 ) ? "#Panorama_FreezePanel_DamageGiven_Multi" : "#Panorama_FreezePanel_DamageGiven" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::PopulateAvatarInfo( const CSteamID &steamID, int nTeam, float flHealth )
{
	static const panorama::CPanoramaSymbol k_symAvatarHidden( "FreezePanel__Avatar--Hidden" );
	static const panorama::CPanoramaSymbol k_symHealthHidden( "FreezePanel__AvatarHealthBar--Hidden" );
	
	// Avatar image

	if ( steamID.IsValid() )
	{
		m_pAvatarPanel->SetSteamID( steamID ); m_pAvatarSSPanel->SetSteamID( steamID );

		m_pAvatarPanel->RemoveClass( k_symAvatarHidden ); m_pAvatarSSPanel->RemoveClass( k_symAvatarHidden );
		m_pAvatarDefaultTerroristPanel->AddClass( k_symAvatarHidden ); m_pAvatarDefaultTerroristSSPanel->AddClass( k_symAvatarHidden );
		m_pAvatarDefaultCTPanel->AddClass( k_symAvatarHidden ); m_pAvatarDefaultCTSSPanel->AddClass( k_symAvatarHidden );
	}
	else
	{
		m_pAvatarPanel->AddClass( k_symAvatarHidden ); m_pAvatarSSPanel->AddClass( k_symAvatarHidden );
		m_pAvatarDefaultTerroristPanel->SetHasClass( k_symAvatarHidden, ( nTeam != TEAM_TERRORIST ) ); m_pAvatarDefaultTerroristSSPanel->SetHasClass( k_symAvatarHidden, ( nTeam != TEAM_TERRORIST ) );
		m_pAvatarDefaultCTPanel->SetHasClass( k_symAvatarHidden, ( nTeam != TEAM_CT ) ); m_pAvatarDefaultCTSSPanel->SetHasClass( k_symAvatarHidden, ( nTeam != TEAM_CT ) );
	}

	// Health bar

	if ( flHealth < 0.0f )
	{
		m_pAvatarHealthPanel->AddClass( k_symHealthHidden );
		m_pAvatarHealthSSPanel->AddClass( k_symHealthHidden );
	}
	else
	{
		m_pAvatarHealthPanel->RemoveClass( k_symHealthHidden );
		m_pAvatarHealthPanel->SetValue( flHealth );

		m_pAvatarHealthSSPanel->RemoveClass( k_symHealthHidden );
		m_pAvatarHealthSSPanel->SetValue( flHealth );
	}

	// Medals
	// Medals set in javascript as it is easier to call Inventory/Friendslist ui component
	// functions. Convert to C++ if necessary (currently not on the hot path)

	XUID xuid = steamID.ConvertToUint64();
	static char xuidText[255] = {};
	V_snprintf( xuidText, ARRAYSIZE( xuidText ), "%llu", xuid );

	DispatchEvent( CSGOHudFreezePanelSetFlair(), this, xuidText );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::PopulateFreezeFrameBorder( const C_CSPlayer *pLocalPlayer )
{
	static const panorama::CPanoramaSymbol k_symFreezeFrameBorderIndex( "FreezeFrameBorderIndex" );
	static const panorama::CPanoramaSymbol k_symFreezeFrameBorderNone( "FreezePanelRoot--BorderNone" );
	
	bool bShouldShowBorder = false;
	if ( bShouldShowBorder && ( m_nFreezeFrameBorderCount > 0 ) && ( m_iKillerIndex >= 0 ) )
	{
		int nIndex = m_iKillerIndex % m_nFreezeFrameBorderCount;
		SwitchClass( k_symFreezeFrameBorderIndex, CFmtStr( "FreezePanelRoot--Border%d", nIndex ).String() );
	}
	else
	{
		SwitchClass( k_symFreezeFrameBorderIndex, k_symFreezeFrameBorderNone );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::PositionPanel()
{
	if ( !m_bIsFreezePanelVisible )
	{
		return;
	}

	int nPrevFreezePanelPosY = m_nFreezePanelPosY;

	if ( cl_freezecampanel_position_dynamic.GetInt() > 0 )
	{
		C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
		if ( pPlayer == NULL )
		{
			return;
		}

		// TODO: get panels that this might overlap with, get their height and make sure they don't overlap
		// need to be able to get the height/width of a panel for this work to be done

		bool bWinPanelVisible = false;
		CCSGO_HudWinPanel * pWinPanel = GET_HUDELEMENT( CCSGO_HudWinPanel );
		if ( pWinPanel && pWinPanel->IsVisible() )
		{
			bWinPanelVisible = true;
		}

		bool bMsgBoxVisible = false;
		// TODO What is the panorama equivalent ?
		/*SFHudInfoPanel *pElement = dynamic_cast<SFHudInfoPanel*>( GetHud().FindElement( "SFHudInfoPanel" ) );
		if ( pElement )
			bMsgBoxVisible = pElement->IsVisible();*/

		float flInterp = pPlayer->GetFreezeFrameInterpolant();

		if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		{
			// we're in replay, position the panel so that it plays nicely with the Replay framing
			m_nFreezePanelPosY = ScreenHeight() * spec_freeze_panel_replay_position.GetFloat();
		}
		else if ( flInterp < 1.0f )
		{
			// Reposition the callout based on our target's position
			Vector vecTarget = pPlayer->GetRenderOrigin();
			CCSPlayer *pKiller = dynamic_cast<CCSPlayer*>( m_FollowEntity.Get() );
			if ( pKiller )
			{
				vecTarget = pKiller->EyePosition();
				vecTarget.z -= 50;
			}
			ASSERT_LOCAL_PLAYER_RESOLVABLE();
			Vector vDelta = vecTarget - MainViewOrigin( GET_ACTIVE_SPLITSCREEN_SLOT() );
			//float flDistance = vDelta.Length();
			VectorNormalize( vDelta );	// Only necessary so we can use it as part of our alpha calculation

										// Is the target visible on screen?
			int iX, iY;
			bool bOnscreen = GetVectorInScreenSpace( vecTarget, iX, iY );

			// some nasty hardcoded numbers until we can get the height/width of the panels surrounding it
			int nMaxY = ScreenHeight() * 0.8;
			if ( bMsgBoxVisible )
			{
				m_nFreezePanelPosY = ( ScreenHeight()*0.75 );
			}
			else if ( bWinPanelVisible || !bOnscreen )
			{
				m_nFreezePanelPosY = ( ScreenHeight()*0.5 );
			}
			else
			{
				m_nFreezePanelPosY = clamp( iY, ScreenHeight()*0.55, nMaxY );
			}

			m_nFreezePanelPosY = Lerp( flInterp, nPrevFreezePanelPosY, m_nFreezePanelPosY );
		}
	}
	else
	{
		m_nFreezePanelPosY = ( ScreenHeight()*0.5 );
	}

	if ( m_nFreezePanelPosY != nPrevFreezePanelPosY )
	{
		CUtlVector<panorama::CTransform3D *> vecTransforms;
		
		// Convert to panorama space (ie 1920x1080)
		float flScaleFactor = 1080.0f / (float)ScreenHeight();
		float flPanoramaY = m_nFreezePanelPosY * flScaleFactor;

		vecTransforms.AddToTail( new panorama::CTransformTranslate3D( 0.0f, flPanoramaY, 0.0f ) );
		m_pFreezePanel->SetTransform3D( vecTransforms );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Read variable from CSS file
//-----------------------------------------------------------------------------
void CCSGO_HudFreezePanel::GetLayoutDefines()
{
	m_nFreezeFrameBorderCount = GetLayoutFileDefineInt( "cppFreezeFrameBorderCount", 0 );
}


//-----------------------------------------------------------------------------
// Purpose: CSS potentially reloaded
//-----------------------------------------------------------------------------
bool CCSGO_HudFreezePanel::OnStyleFileReloaded( panorama::CPanoramaSymbol symFile )
{
	GetLayoutDefines();

	// let bubble
	return false;
}