//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Display csgo death notice panel
//
// SF differences :
//		* Missing additive blending on text
//		* radial gradient on normal death notices - currently png
//		* icons using png - change to SVG ???
//		* ProcessInput throttle in SF - check panorama perf
//		* SF is using a cache of death notice panels - check panorama perf
//		* Missing spectator info - cl#4044690
//
//=============================================================================//

#include "cbase.h"
#include "csgo_huddeathnotice.h"

#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include <engine/IEngineSound.h>
#include "hltvreplaysystem.h"

//#include "uicomponents/uicomponent_overwatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_HudDeathNotice, CSGOHudDeathNotice );


extern ConVar mp_display_kill_assists;
extern ConVar cl_show_clan_in_death_notice;
extern ConVar cl_drawhud_force_deathnotices;
extern ConVar cl_drawhud;

extern bool IsTakingAFreezecamScreenshot();		// TODO Currently defined in sfhudfreezepanel.cpp


#define DEATH_NOTICE_NAME_TRUNCATE_AT			22  // number of name character displayed before truncation
#define DEATH_NOTICE_ASSIST_NAME_TRUNCATE_AT	18  // number of name character displayed before truncation
#define DEATH_NOTICE_ASSIST_SHORT_NAME_TRUNCATE_AT	12  // number of name character displayed before truncation
#define DEATH_NOTICE_MAX_PENDING				15	// Max number of pending death notices


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudDeathNotice::CCSGO_HudDeathNotice( panorama::CPanel2D *pParent, const char *pchID )
:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudDeathNotice", this ),
	m_bVisible( false )
{
	SetHiddenBits( HIDEHUD_MISCSTATUS );
	
	RequireLoadLayout( "file://{resources}/layout/hud/huddeathnotice.xml" );

	m_pVisibleNotices = RequireChildInLayoutFile( "VisibleNotices" );

	RegisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_HudDeathNotice::OnStyleFileReloaded );

	// Panel not visible to start with
	SetVisible( false );

	GetLayoutDefines();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudDeathNotice::~CCSGO_HudDeathNotice()
{
	UnregisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_HudDeathNotice::OnStyleFileReloaded );
	
	StopListeningForAllEvents();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::LevelInit( void )
{
	ClearNotices();
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "hltv_replay" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::LevelShutdown( void )
{
	StopListeningForAllEvents();
	ClearNotices();
}


//-----------------------------------------------------------------------------
// Purpose: Return true if this hud element should be visible in the current hud state
//-----------------------------------------------------------------------------
bool CCSGO_HudDeathNotice::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return ( cl_drawhud_force_deathnotices.GetInt() >= 0 ) && (
		( cl_drawhud_force_deathnotices.GetInt() > 0 ) ||
		cl_drawhud.GetBool()
		) && CHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::SetActive( bool bActive )
{
	if ( bActive != m_bVisible )
	{
		ShowPanel( bActive );
	}

	if ( bActive == false && m_bActive == true )
	{
		// We want to continue to run ProcessInput while the HUD element is hidden
		// so that the notifications continue advancing down the screen
		return;
	}

	CHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::ShowPanel( bool bShow )
{
	if ( bShow != m_bVisible )
	{
		SetVisible( bShow );
		m_bVisible = bShow;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update visible death notices
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::ProcessInput( void )
{
	static const panorama::CPanoramaSymbol k_symSpawnTime( "SpawnTime" );
	static const panorama::CPanoramaSymbol k_symLifetime( "Lifetime" );
	static const panorama::CPanoramaSymbol k_symLifetimeMod( "LifetimeMod" );
	static const panorama::CPanoramaSymbol k_symFadeOut( "FadeOut" );
	static const panorama::CPanoramaSymbol k_symAvailable( "Available" );

	// make sure we always have full notice panel in queue
	int nNumToCreate = DEATH_NOTICE_MAX_PENDING - m_pVisibleNotices->GetChildCount();
	for ( int i=0; i<nNumToCreate; ++i )
	{
		panorama::CPanel2D *pNewPanel = new panorama::CPanel2D( m_pVisibleNotices, nullptr );
		pNewPanel->SetHasClass( k_symAvailable, true );
		pNewPanel->RequireLoadLayoutSnippet( "DeathNotice" );
	}

	// Remove old death notices
	for ( CPanel2D *pDeathNotice : m_pVisibleNotices->Children() )
	{
		if ( pDeathNotice->BHasClass( k_symAvailable ) )
		{
			continue;
		}

		// make sure it's not already in async delete and not available panel that's waiting to be initialized
		if ( !panorama::UIEngine()->BIsPanelWaitingAsyncDelete( pDeathNotice->UIPanel() ) )
		{
			float fAge = gpGlobals->curtime - pDeathNotice->GetAttribute( k_symSpawnTime, 0.0f );

			if ( fAge > ( pDeathNotice->GetAttribute( k_symLifetime, 0.0f ) * pDeathNotice->GetAttribute( k_symLifetimeMod, 0.0f ) ) )
			{
				// Start fade out
				pDeathNotice->AddClass( k_symFadeOut );
				pDeathNotice->DeleteAsync( m_nFadeOutTime );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	static ConVarRef spec_replay_enable( "spec_replay_enable" );

	if ( !V_strcmp( type, "player_death" ) )
	{
		if ( ( spec_replay_enable.GetInt() == 2 )			// permanent replay mode routes death events to us appropriately
			|| !g_HltvReplaySystem.GetHltvReplayDelay()		// live gameplay (not in replay) shows all death events
			|| event->GetBool( "realtime_passthrough" ) )	// realtime passthrough event was delivered to us explicitly and we should show it
			OnPlayerDeath( event );
	}
	else if ( !V_strcmp( type, "hltv_replay" ) )
	{
		int numSecondsInKillerReplayDelay = event->GetInt( "delay" );
		if ( ( numSecondsInKillerReplayDelay != 0 ) && ( spec_replay_enable.GetInt() == 2 ) )
		{
			// We are entering a permanent killer replay, wipe our death notices list
			ClearNotices();
		}
	}
}

// For backwards compatibility with old demos
struct DeathNoticeEventWeaponRename
{
	const char* fromName;
	const char* toName;
};

static const DeathNoticeEventWeaponRename sRenames[] = {
	{ "knife_default_ct", "knife" },
	{ "knife_default_t", "knife_t" },
	{ "galil", "galilar" },
	{ "molotov_projectile_impact", "inferno" },
};

//-----------------------------------------------------------------------------
// Purpose: "player_death" event handler
//			Adding new death notice to the pending notices panel
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::OnPlayerDeath( IGameEvent *event )
{
	//
	// Data used to setup death notice
	//
	
	bool bHeadshot = false;
	bool bSuicide = false;
	bool bDominated = false;
	bool bRevenge = false;
	bool isVictim = false;
	bool bPenetrated = false;
	bool isKiller = false;
	int nVictimTeam = 0;
	int nAtackerTeam = 0;
	int nAssisterTeam = 0;
	const char * szWeapon = nullptr;


	// 
	// Collect data from event
	// 

	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

	if ( !cs_PR )
	{
		Assert( false );
		return;
	}

	bool bPlayingSurvival = CSGameRules() && CSGameRules()->IsPlayingSurvival();

	int nAttacker = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
	int nVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
	int nAssister = mp_display_kill_assists.GetBool() ? engine->GetPlayerForUserID( event->GetInt( "assister" ) ) : 0;

	if ( bPlayingSurvival )
		nAssister = 0;

	szWeapon = event->GetString( "weapon" );
	if ( szWeapon == NULL || szWeapon[0] == 0 )
		return;

	bHeadshot = event->GetInt( "headshot" ) > 0;
	bSuicide = ( nAttacker == 0 || nAttacker == nVictim );
	bDominated = false;
	bRevenge = false;
	isVictim = GetLocalPlayerIndex() == nVictim;
	bPenetrated = ( event->GetInt( "penetrated" ) > 0 );
	isKiller = !isVictim && ( GetLocalPlayerIndex() == nAttacker || GetLocalPlayerIndex() == nAssister ); //Need to check isVictim because the player is both killer and victim in a suicide

	wchar_t wszAttackerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
	wchar_t wszVictimName[MAX_DECORATED_PLAYER_NAME_LENGTH];
	wchar_t wszAssisterName[MAX_DECORATED_PLAYER_NAME_LENGTH];

	wszAttackerName[0] = L'\0';
	wszVictimName[0] = L'\0';
	wszAssisterName[0] = L'\0';

	EDecoratedPlayerNameFlag_t eDecorationFlags = k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot;
	if ( cl_show_clan_in_death_notice.GetInt() <= 0 )
		eDecorationFlags |= k_EDecoratedPlayerNameFlag_DontShowClanName;

	if ( nAttacker > 0 )
	{
		cs_PR->GetDecoratedPlayerName( nAttacker, wszAttackerName, sizeof( wszAttackerName ), eDecorationFlags );
	}

	if ( nVictim > 0 )
	{
		cs_PR->GetDecoratedPlayerName( nVictim, wszVictimName, sizeof( wszVictimName ), eDecorationFlags );
	}

	if ( nAssister > 0 )
	{
		// if our attacker is the same as our assiter, it means a bot attacked the victim and a player took over that bot
		if ( nAssister == nAttacker )
			nAssister = 0;
		else
		{
			cs_PR->GetDecoratedPlayerName( nAssister, wszAssisterName, sizeof( wszAssisterName ), eDecorationFlags );
		}
	}

	// Highlight "The Suspect" in death feed
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID && pParameters->m_bAnonymousPlayerIdentity )
		{
			static wchar_t const * const kwszTheSuspect = g_pVGuiLocalize->Find( "#CSGO_Overwatch_TheSuspect" );
			Assert( kwszTheSuspect );

			// Setup victim/killer around death notice
			if ( !isVictim && !isKiller )
			{
				if ( V_wcsistr( wszVictimName, kwszTheSuspect ) )
					isVictim = true;
				else if ( V_wcsistr( wszAttackerName, kwszTheSuspect ) )
				{
					isKiller = true;


					// When suspect gets kills play a sound to draw Overwatcher's attention
					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "UI.DeathNotice" );

				}
				else if ( V_wcsistr( wszAssisterName, kwszTheSuspect ) )
					isKiller = true;
			}
		}
		else if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			player_info_t pinfo;
			engine->GetPlayerInfo( nAttacker, &pinfo );
			if ( pParameters->m_uiLockFirstPersonAccountID == CSteamID( pinfo.xuid ).GetAccountID() )
			{
				isKiller = true;
			}
		}
	}

	if ( bSuicide )
	{
		STEAMWORKS_TESTSECRETALWAYS();
	}
	nVictimTeam = nVictim > 0 ? cs_PR->GetTeam( nVictim ) : 0;
	nAtackerTeam = nAttacker > 0 ? cs_PR->GetTeam( nAttacker ) : 0;
	nAssisterTeam = nAssister > 0 ? cs_PR->GetTeam( nAssister ) : 0;

	CCSPlayer* pCSPlayerKiller = ToCSPlayer( ClientEntityList().GetBaseEntity( nAttacker ) );
	CCSPlayer* pCSPlayerAssister = ToCSPlayer( ClientEntityList().GetBaseEntity( nAssister ) );

	if ( event->GetInt( "dominated" ) > 0 || ( pCSPlayerKiller != NULL && pCSPlayerKiller->IsPlayerDominated( nVictim ) ) )
	{
		bDominated = true;
	}
	else if ( event->GetInt( "revenge" ) > 0 )
	{
		bRevenge = true;
	}

	C_CSPlayer *pLocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalCSPlayer )
		return;

	bool bAssisterIsTarget = pCSPlayerAssister && pCSPlayerAssister->IsAssassinationTarget();
	bool bAttackerIsTarget = pCSPlayerKiller && pCSPlayerKiller->IsAssassinationTarget();

	if ( nAssister > 0 )
	{
		if ( !bAttackerIsTarget )
			TruncatePlayerName( wszAttackerName, ARRAYSIZE( wszAttackerName ), DEATH_NOTICE_ASSIST_NAME_TRUNCATE_AT );

		if ( !bAssisterIsTarget )
			TruncatePlayerName( wszAssisterName, ARRAYSIZE( wszAssisterName ), DEATH_NOTICE_ASSIST_SHORT_NAME_TRUNCATE_AT );
	}
	else
	{
		if ( !bAttackerIsTarget )
			TruncatePlayerName( wszAttackerName, ARRAYSIZE( wszAttackerName ), DEATH_NOTICE_NAME_TRUNCATE_AT );
	}

	CCSPlayer* pCSPlayerVictim = ToCSPlayer( ClientEntityList().GetBaseEntity( nVictim ) );
	bool bVictimIsTarget = pCSPlayerVictim && pCSPlayerVictim->IsAssassinationTarget();
	if ( !bVictimIsTarget )
	{
		if ( nAssister > 0 )
			TruncatePlayerName( wszVictimName, ARRAYSIZE( wszVictimName ), DEATH_NOTICE_ASSIST_NAME_TRUNCATE_AT );
		else
			TruncatePlayerName( wszVictimName, ARRAYSIZE( wszVictimName ), DEATH_NOTICE_NAME_TRUNCATE_AT );
	}

	// Skip leading "weapon_" or "item_" in weapon name
	if ( const char* szWeaponAfter = StringAfterPrefix( szWeapon, "weapon_" ) )
		szWeapon = szWeaponAfter;
	if ( const char* szWeaponAfter = StringAfterPrefix( szWeapon, "item_" ) )
		szWeapon = szWeaponAfter;

	// Rename event strings from old demos and stuff to match our existing equipment & icons
	for ( int i = 0; i < V_ARRAYSIZE( sRenames ); ++i )
	{
		if ( !V_strcmp( szWeapon, sRenames[i].fromName ) )
			szWeapon = sRenames[i].toName;
	}

	// Special case for the danger zone controller
	bool bVictimsAttackerIsSuicide = bSuicide;
	if ( bVictimsAttackerIsSuicide )
	{
		if ( !V_strcmp( szWeapon, "dangerzone_controller" ) )
		{
			bVictimsAttackerIsSuicide = false;
			const wchar_t *pwszDangerZone = g_pVGuiLocalize->Find( "#SFUI_WPNHUD_DangerZone" );
			V_wcscpy_safe( wszAttackerName, pwszDangerZone );
		}
		else if ( !V_strcmp( szWeapon, "env_gunfire" ) )
		{
			bVictimsAttackerIsSuicide = false;
			const wchar_t *pwszSentry = g_pVGuiLocalize->Find( "#SFUI_WPNHUD_AutoSentry" );
			V_wcscpy_safe( wszAttackerName, pwszSentry );
		}
	}

	// Convert attacker, assister and victim to UTF-8 strings
	// Each Unicode code point can expand to as many as four bytes in UTF-8

	char szAttackerNameUTF8[ 4 * MAX_DECORATED_PLAYER_NAME_LENGTH ];
	char szVictimNameUTF8[ 4 * MAX_DECORATED_PLAYER_NAME_LENGTH ];
	char szAssisterNameUTF8[ 4 * MAX_DECORATED_PLAYER_NAME_LENGTH ];
	V_UnicodeToUTF8( wszAttackerName, szAttackerNameUTF8, ARRAYSIZE( szAttackerNameUTF8 ) );
	V_UnicodeToUTF8( wszVictimName, szVictimNameUTF8, ARRAYSIZE( szVictimNameUTF8 ) );
	V_UnicodeToUTF8( wszAssisterName, szAssisterNameUTF8, ARRAYSIZE( szAssisterNameUTF8 ) );

	//
	// Add new death notice to the pending notices panel
	//

	char szWeaponPath[64];
	V_snprintf( szWeaponPath, ARRAYSIZE( szWeaponPath ), "file://{images}/icons/equipment/%s.svg", szWeapon );

	static const panorama::CPanoramaSymbol k_symDeathNoticeVictim( "DeathNotice_Victim" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeKiller( "DeathNotice_Killer" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeCTColor( "DeathNoticeCTColor" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeFadedColor( "DeathNoticeFadedColor" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeTColor( "DeathNoticeTColor" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeRevenge( "DeathNoticeRevenge" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeDomination( "DeathNoticeDomination" );
	static const panorama::CPanoramaSymbol k_symDeathNoticePenetrate( "DeathNoticePenetrate" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeHeadShot( "DeathNoticeHeadShot" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeSuicide( "DeathNoticeSuicide" );
	static const panorama::CPanoramaSymbol k_symDeathNoticeAssist( "DeathNoticeAssist" );
	static const panorama::CPanoramaSymbol k_symSpawnTime( "SpawnTime" );
	static const panorama::CPanoramaSymbol k_symLifetime( "Lifetime" );
	static const panorama::CPanoramaSymbol k_symLifetimeMod( "LifetimeMod" );
	static const panorama::CPanoramaSymbol k_symAvailable( "Available" );

	// find first available panel to display
	panorama::CPanel2D *pDeathNotice = NULL;
	for ( CPanel2D *pChild : m_pVisibleNotices->Children() )
	{
		if ( pChild->BHasClass( k_symAvailable ) )
		{
			pChild->SetHasClass( k_symAvailable, false );
			pDeathNotice = pChild;
			break;
		}
	}

	// if we didn't find anything available, remove the oldest one and add new one
	if ( !pDeathNotice )
	{
		delete m_pVisibleNotices->GetFirstChild();

		pDeathNotice = new panorama::CPanel2D( m_pVisibleNotices, nullptr );
		pDeathNotice->RequireLoadLayoutSnippet( "DeathNotice" );
	}
	
	// Set attacker/assister/victim text and color
	panorama::CLabel *pAttackerLabel = panorama::panel_cast< panorama::CLabel * >( pDeathNotice->RequireChildInLayoutFile( "Attacker" ) );
	pAttackerLabel->SetDialogVariable( "name", bVictimsAttackerIsSuicide ? szVictimNameUTF8 : szAttackerNameUTF8 );
	pAttackerLabel->SetHasClass( k_symDeathNoticeCTColor, ( nAtackerTeam == TEAM_CT ) );
	pAttackerLabel->SetHasClass( k_symDeathNoticeTColor, ( nAtackerTeam == TEAM_TERRORIST ) );
	pAttackerLabel->SetHasClass( k_symDeathNoticeFadedColor, false );

	panorama::CLabel *pVictimLabel = panorama::panel_cast<panorama::CLabel *>( pDeathNotice->RequireChildInLayoutFile( "Victim" ) );
	pVictimLabel->SetDialogVariable( "name", szVictimNameUTF8 );
	pVictimLabel->SetHasClass( k_symDeathNoticeCTColor, ( nVictimTeam == TEAM_CT ) && !bPlayingSurvival );
	pVictimLabel->SetHasClass( k_symDeathNoticeTColor, ( nVictimTeam == TEAM_TERRORIST ) && !bPlayingSurvival );
	pVictimLabel->SetHasClass( k_symDeathNoticeFadedColor, bPlayingSurvival );

	panorama::CLabel *pAssisterLabel = panorama::panel_cast<panorama::CLabel *>( pDeathNotice->RequireChildInLayoutFile( "Assister" ) );
	pAssisterLabel->SetDialogVariable( "name", szAssisterNameUTF8 );
	pAssisterLabel->SetHasClass( k_symDeathNoticeCTColor, ( nAssisterTeam == TEAM_CT ) );
	pAssisterLabel->SetHasClass( k_symDeathNoticeTColor, ( nAssisterTeam == TEAM_TERRORIST ) );
	pAssisterLabel->SetHasClass( k_symDeathNoticeFadedColor, false );

	panorama::CImagePanel *pWeaponIcon = panorama::panel_cast< panorama::CImagePanel * >( pDeathNotice->RequireChildInLayoutFile( "Weapon" ) );
	pWeaponIcon->SetImageJS( szWeaponPath );
	
	// Change death notice style
	pDeathNotice->SetHasClass( k_symDeathNoticeVictim, isVictim );
	pDeathNotice->SetHasClass( k_symDeathNoticeKiller, isKiller );

	// Show / hide icons
	pDeathNotice->SetHasClass( k_symDeathNoticeRevenge, bRevenge );
	pDeathNotice->SetHasClass( k_symDeathNoticeDomination, bDominated );
	pDeathNotice->SetHasClass( k_symDeathNoticePenetrate, bPenetrated );
	pDeathNotice->SetHasClass( k_symDeathNoticeHeadShot, bHeadshot );
	pDeathNotice->SetHasClass( k_symDeathNoticeSuicide, bSuicide );
	pDeathNotice->SetHasClass( k_symDeathNoticeAssist, ( nAssister > 0 ) );

	// Set spawn time and lifetime attributes
	pDeathNotice->SetAttribute( k_symSpawnTime, gpGlobals->curtime );
	pDeathNotice->SetAttribute( k_symLifetime, m_nNoticeLifetime );
	pDeathNotice->SetAttribute( k_symLifetimeMod, ( ( isVictim || isKiller ) ? m_nLocalPlayerLifetimeMod : 1.0f ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::ClearNotices()
{
	for ( CPanel2D *pDeathNotice : m_pVisibleNotices->Children() )
	{
		pDeathNotice->DeleteAsync();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Read variable from CSS file
//-----------------------------------------------------------------------------
void CCSGO_HudDeathNotice::GetLayoutDefines()
{
	m_nNoticeLifetime = GetLayoutFileDefineFloat( "DeathNoticeLifetime", 5.0f );
	m_nLocalPlayerLifetimeMod = GetLayoutFileDefineFloat( "DeathNoticeLocalPlayerLifetimeMod", 1.5f );
	m_nFadeOutTime = GetLayoutFileDefineFloat( "DeathNoticeFadeOutTime", 1.0f );
}


//-----------------------------------------------------------------------------
// Purpose: CSS potentially reloaded
//-----------------------------------------------------------------------------
bool CCSGO_HudDeathNotice::OnStyleFileReloaded( panorama::CPanoramaSymbol symFile )
{
	GetLayoutDefines();

	// let bubble
	return false;
}
