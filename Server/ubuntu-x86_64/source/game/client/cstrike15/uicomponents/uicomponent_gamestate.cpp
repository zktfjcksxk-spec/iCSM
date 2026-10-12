//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Component for playerresource access. Only used by panorama and contains match and player gameplay info.
//
//=============================================================================//

#include "cbase.h"

#include "uicomponent_gamestate.h"
#include "c_cs_playerresource.h"
#include "c_team.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "gametypes/igametypes.h"
#include "matchmaking/mm_helpers.h"
//#include "uicomponent_matchstats.h"
#include "panorama/hud/csgo_hud.h"
#include "c_user_message_register.h"
//#include "gc_clientsystem.h"
#include "voice_status.h"
#include "hltvcamera.h"
#include "hud_element_helper.h"
#include "cs_hud_chat.h"
#include "dangerzone_controller.h"

#if defined ( PANORAMA_ENABLE )
#include "panorama/hud/csgo_hudvoicestatus.h"
#endif

#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//////////////////////////////////////////////////////////////////////////
//
// Component instance
//
#if defined ( PANORAMA_ENABLE )
	DEFINE_PANORAMA_EVENT( GameState_UpdatePlayer )
	DEFINE_PANORAMA_EVENT( GameState_UpdateAllPlayers )
	DEFINE_PANORAMA_EVENT( GameState_OnLevelLoad )
	DEFINE_PANORAMA_EVENT( GameState_OnMatchStart )
	DEFINE_PANORAMA_EVENT( GameState_ServerRankUpdate )
	DEFINE_PANORAMA_EVENT( GameState_ServerSpawn )
	DEFINE_PANORAMA_EVENT( BackupFileNamesReceived )
	DEFINE_PANORAMA_EVENT( GameState_CommendPlayerQueryResponse )
	DEFINE_PANORAMA_EVENT( GameState_RankRevealAll )
#endif
		 
// No SF impl needed, but create the global object so the shared macros don't complain
SF_COMPONENT_API_DEF_BEGIN( CUiComponent_GameState )
SF_COMPONENT_API_DEF_END( CUiComponent_GameState )

#if defined ( PANORAMA_ENABLE )

PANORAMA_COMPONENT_API_DEF_BEGIN( CUiComponent_GameState )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname, argNames, description ) PANORAMA_COMPONENT_FUNCTION_API_DEF_DOC( returntype, fnname, CUiComponent_GameState, argNames, description )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( returntype, fnname, description ) PANORAMA_COMPONENT_FUNCTION_RAW_API_DEF_DOC( returntype, fnname, CUiComponent_GameState, description )
#include "uicomponent_gamestate.functions.inc"
#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT
#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW

PANORAMA_COMPONENT_API_DEF_END( CUiComponent_GameState )

#endif	// PANORAMA_ENABLE

UI_COMPONENT_API_DEF_COMMON( CUiComponent_GameState, GameState ) // note: macro adds postfix 'API'


//////////////////////////////////////////////////////////////////////////
//
// Component Implementation
//

CUiComponent_GameState::CUiComponent_GameState()
{
	m_bServerRankRevealAll = false;

	m_iFreeForAllModeWinnerIndex = -1;

	m_flTimeWhenShudownHappens = 0;
	m_flTimeWhenClientShouldDropFromGotv = 0;

	m_szLocalId = "";
	
}

CUiComponent_GameState::~CUiComponent_GameState()
{
}


static XUID GetXuidFromXuidString( const char * szXuid )
{
	
	XUID xuid = ConvertToUint64( szXuid ); // validates the Steam ID as well

	return xuid;

}

XUID CUiComponent_GameState::GetPlayerXuidFromEntIndex( int nEntIndex )
{
	Assert( CSGameRules() );

	if ( !GetGameState() )
		return 0;


	return GetExtraState()->GetXuid( nEntIndex );
}


static int GetGamePhase()
{
	// GAMEPHASE_WARMUP_ROUND			0
	// GAMEPHASE_PLAYING_STANDARD		1
	// GAMEPHASE_PLAYING_FIRST_HALF		2
	// GAMEPHASE_PLAYING_SECOND_HALF	3
	// GAMEPHASE_HALFTIME				4
	// GAMEPHASE_MATCH_ENDED			5

	int nGamePhase = -1;
	if ( CCSGameRules *mp = CSGameRules() )
	{
		if ( mp->IsWarmupPeriod() )
			nGamePhase = GAMEPHASE_WARMUP_ROUND;
		else
			nGamePhase = mp->GetGamePhase();
	}

	return nGamePhase;
}

///////////////////
///
///	Player data

#if defined ( PANORAMA_ENABLE )

// a javascript-exposed method that returns a JSO
//
void CUiComponent_GameState::GetPlayerDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( !GetGameState() )
		return;

	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );
	v8::Local< v8::Object > pObject = v8::Object::New( args.GetIsolate() );

	v8::Local< v8::Object > teamObjects[MAX_TEAMS];

	for ( int idxPlayer = 1; idxPlayer <= gpGlobals->maxClients; idxPlayer++ )
	{
		// skip hltv proxies
		if ( GetExtraState()->IsHLTV( idxPlayer ) )
			continue;

		if ( !GetGameState()->IsConnected( idxPlayer ) )
			continue;

		int nTeam = GetGameState()->GetTeam( idxPlayer );
		if ( nTeam < 0 || nTeam >= MAX_TEAMS )
			continue;

		if ( teamObjects[nTeam].IsEmpty() )
		{
			// JS: result[teamName] = {};
			teamObjects[nTeam] = v8::Object::New( args.GetIsolate() );

			const char *szTeamName = "unknown";
			if ( nTeam == TEAM_UNASSIGNED )
				szTeamName = "UNASSIGNED";
			else if ( C_Team *pTeam = GetGlobalTeam( nTeam ) )	// TODO: Do we need to latch any team data?  I think we don't
				szTeamName = pTeam->Get_Name();

			pObject->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( args.GetIsolate(), szTeamName ).ToLocalChecked(), teamObjects[nTeam] );
		}

		// JS: result[teamName][idxPlayer] = GetPlayerXuidFromIndex(idxPlayer);
		CreateJSOEntry_String( teamObjects[ nTeam ], CFmtStr( "%d", idxPlayer ), GetPlayerXuidStringFromEntIndex( idxPlayer ) );
	}

	args.GetReturnValue().Set( pObject );
}

#endif	// PANORAMA_ENABLE





int CUiComponent_GameState::GetTeamTotalPlayerCount( const char * szTeamname )
{
	if ( !GetCSResources() )
		return -1;

	if ( !szTeamname )
		return -1;

	C_Team* pTeam = nullptr;

	for ( int iTeam = 0; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		pTeam = GetGlobalTeam( iTeam );

		if ( !V_strcmp( pTeam->Get_Name(), szTeamname ) )
			break;
	}

	if ( !pTeam )
		return -1;


	return pTeam->GetNumPlayers();

}

int CUiComponent_GameState::GetTeamLivingPlayerCount( const char * szTeamname )
{
	if ( !GetCSResources() )
		return -1;

	if ( !szTeamname )
		return -1;

	C_Team* pTeam = nullptr;

	for ( int iTeam = 0; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		pTeam = GetGlobalTeam( iTeam );

		if ( !V_strcmp( pTeam->Get_Name(), szTeamname ) )
			break;
	}

	if ( !pTeam )
		return -1;

	int nTeam = pTeam->GetTeamNumber();

	int iNumAlive = 0;

	for ( int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++ )
	{
		if ( GetCSResources()->IsConnected( playerIndex ) )
		{
			int controlledBy = GetCSResources()->GetControlledByPlayer( playerIndex );
			int controlling = GetCSResources()->GetControlledPlayer( playerIndex );
			if ( GetCSResources()->GetTeam( playerIndex ) == nTeam &&
				( controlledBy || ( g_PR->IsAlive( playerIndex ) && !controlling ) ) )
			{
				iNumAlive++;
			}
		}
	}

	return iNumAlive;
}




// given a team name (e.g. "TERRORIST") return the clan name (e.g. "FNATIC") if it exists.
// otherwise return the team name ( e.g. "TERRORIST" ) localized

const char * CUiComponent_GameState::GetTeamClanName( const char * szTeamname )
{
	if ( !szTeamname )
		return "";

	for ( int iTeam = 0; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		C_Team *pTeam = GetGlobalTeam( iTeam );

		if ( !V_strcmp( pTeam->Get_Name(), szTeamname ) )
		{
			if ( pTeam->Get_ClanName()[ 0 ] )
			{
				return pTeam->Get_ClanName();
			}
			else if ( const locchar_t *szLocName = g_pVGuiLocalize->Find( CFmtStr( "teamname_%s", szTeamname ) ) )
			{
				// return generic team name
				static char szTeamNameUTF8[MAX_TEAM_NAME_LENGTH*4]; // Unicode chars can be up to 4 bytes per char  
				V_UnicodeToUTF8( szLocName, szTeamNameUTF8, ARRAYSIZE( szTeamNameUTF8 ) );
				return szTeamNameUTF8;
			}
			else
			{
				return szTeamname; // we don't have a loc string for this team name, fallback to unlocalized
			}
		}
	}

	return "";
}


const char * CUiComponent_GameState::GetTeamLogoImagePath( const char * szTeamname )
{
	if ( !szTeamname )
		return "";


	for ( int iTeam = 0; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		C_Team *pTeam = GetGlobalTeam( iTeam );

		if ( !V_strcmp( pTeam->Get_Name(), szTeamname ) )
		{

			static CUtlString s_strImagePath;

			if ( pTeam->Get_LogoImageString()[ 0 ] )
			{
				s_strImagePath.Format( "/tournaments/teams/%s.svg", pTeam->Get_LogoImageString() );
			}
// 			else if ( pTeam->Get_FlagImageString()[ 0 ] )
// 			{
// 				s_strImagePath.Format( "/icons/flags/%s.svg", pTeam->Get_FlagImageString() );
// 			}
			else // default T / CT images
			{
				if ( !V_strcmp( pTeam->Get_Name(), "CT" ) )
				{
					s_strImagePath.Format( "/icons/ct_logo.svg" );
				}
				else if ( !V_strcmp( pTeam->Get_Name(), "TERRORIST" ) )
				{
					s_strImagePath.Format( "/icons/t_logo.svg" );
				}
			}

			return s_strImagePath.Get();
		}
	}

	return "";

}


const char * CUiComponent_GameState::GetTeamGungameLeaderXuid( const char * szTeamname )
{
	if ( !szTeamname )
		return "";

	for ( int iTeam = 0; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		C_Team *pTeam = GetGlobalTeam( iTeam );

		if ( !V_strcmp( pTeam->Get_Name(), szTeamname ) )
		{
			int nEntIdx = pTeam->GetGGLeader( iTeam );

			if ( nEntIdx != -1 )
			{
				return GetPlayerXuidStringFromEntIndex( nEntIdx );
			}
		}
	}

	return "";
}



const char * CUiComponent_GameState::GetTeamFlagImagePath( const char * szTeamname )
{
	if ( !szTeamname )
		return "";


	for ( int iTeam = 0; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		C_Team *pTeam = GetGlobalTeam( iTeam );

		if ( !V_strcmp( pTeam->Get_Name(), szTeamname ) )
		{
			return pTeam->Get_FlagImageString();
		}
	}

	return "";
}

const char * CUiComponent_GameState::GetMapsInCurrentMapGroup()
{
	return GetMapsInMapGroup( nullptr );
}

const char * CUiComponent_GameState::GetMapsInMapGroup( const char* szMapGroup )
{
	const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( szMapGroup ? szMapGroup : engine->GetMapGroupName() );
	if ( mapsInGroup )
	{
		static CUtlStringBuilder strList;
		strList.Clear();
		for ( int i = 0; i < mapsInGroup->Count(); ++i )
		{
			const char* internalMapName = (*mapsInGroup)[i];
			if ( strList.Length() )
				strList.Append( "," );

			strList.Append( internalMapName );
		}

		return strList.Get(); // when called from JS this will get copied to storage owned by the v8 object, so subsequent calls changing strList shouldn't be a problem.
	}
	else
	{
		return nullptr;
	}
}

const char* CUiComponent_GameState::GetMapDisplayNameToken( const char* szMapName )
{
	static char szOutBuffer[ 128 ];
	if ( CSGameRules() && CSGameRules()->GetFriendlyMapNameToken( szMapName, szOutBuffer, sizeof( szOutBuffer ) ) )
	{
		return szOutBuffer;
	}
	return szMapName;
}

#if defined ( PANORAMA_ENABLE )
void CUiComponent_GameState::GetKickTargets( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );
	v8::Local< v8::Object > pObject = v8::Object::New( args.GetIsolate() );

	for ( int player = 1; player < MAX_PLAYERS; ++player )
	{
		if ( GetCSResources() && GetCSResources()->IsConnected( player ) &&
			 !GetCSResources()->IsFakePlayer( player ) &&
			 GetLocalPlayerIndex() != player &&
			GetCSResources()->GetTeam( player ) == GetCSResources()->GetTeam( GetLocalPlayerIndex() ) )
		{
			player_info_t playerInfo;
			if ( engine->GetPlayerInfo( player, &playerInfo ) )
			{
				pObject->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( args.GetIsolate(), GetCSResources()->GetPlayerName( player ) ).ToLocalChecked(), v8::Number::New( args.GetIsolate(), playerInfo.userID ) );
			}
		}
	}
	args.GetReturnValue().Set( pObject );
}

bool __MsgFunc_RoundBackupFilenames_Panorama( const CCSUsrMsg_RoundBackupFilenames &msg )
{
	// Copy/paste from SF version
	static CCSUsrMsg_RoundBackupFilenames files[ 10 ];

	files[ clamp( msg.index(), 0, 9 ) ] = msg;

	// don't do anything until we get the last file msg.
	if ( msg.index() < ( msg.count() - 1 ) )
		return true;

	char szCommaDelimitedFilenames[ 1024 ] = { 0 };
	char szCommaDelimitedNicenames[ 1024 ] = { 0 };

	for ( int i = 0; i < min( msg.count(), 10 ); i++ )
	{
		if ( *szCommaDelimitedFilenames )
			V_strcat_safe( szCommaDelimitedFilenames, "," );

		V_strcat_safe( szCommaDelimitedFilenames, files[ i ].filename().c_str() );

		if ( *szCommaDelimitedNicenames )
			V_strcat_safe( szCommaDelimitedNicenames, "," );

		V_strcat_safe( szCommaDelimitedNicenames, files[ i ].nicename().c_str() );
	}

	v8::Isolate *pIsolate = panorama::UIEngine()->GetV8Isolate();
	v8::Isolate::Scope isolate_scope( pIsolate );
	v8::HandleScope handle_scope( pIsolate );
	v8::Local<v8::Context> ctx = v8::Context::New( pIsolate );
	v8::Context::Scope context_scope( ctx );
	v8::Local< v8::Object > pObject = v8::Object::New( pIsolate );
	pObject->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "display_names" ).ToLocalChecked(), v8::String::NewFromUtf8( pIsolate, szCommaDelimitedNicenames ).ToLocalChecked() );
	pObject->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "file_names" ).ToLocalChecked(), v8::String::NewFromUtf8( pIsolate, szCommaDelimitedFilenames ).ToLocalChecked() );

	panorama::DispatchEvent( BackupFileNamesReceived(), nullptr, pObject );

	return true;
}
static CUserMessageRegister< CS_UM_RoundBackupFilenames, CCSUsrMsg_RoundBackupFilenames > userMessageRegister_RoundBackupFilenames_Panorama( __MsgFunc_RoundBackupFilenames_Panorama );
#endif

// sometimes we don't care whether it's a bot or an anonymous player, such as when picking an avatar
bool CUiComponent_GameState::IsXuidValid( char const* szXuid )
{
	if ( !GetGameState() )
		return false;

	XUID xuid = GetXuidFromXuidString( szXuid );

	return ( xuid != 0 );
}


int CUiComponent_GameState::GetPlayerIndex( char const* szXuid )
{
	if ( !GetGameState() )
		return 0;

	// NOTE: Don't use GetXuidFromXuidString() here since it validates the steamid,
	//       and we want to get the correct player index for bots/overwatch
	uint64 xuid = Q_atoi64( szXuid );
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		if ( GetExtraState()->GetXuid( i ) == xuid )
			return i;
	}

	// szXuid is not connected to server
	return 0;
}


char const * CUiComponent_GameState::GetPlayerNameSafe( char const* szXuid )
{
	if ( !GetCSResources() )
		return "";

	int nPlayerIndex = GetPlayerIndex( szXuid );
	return GetExtraState()->GetDecoratedPlayerName( nPlayerIndex, true );
}

const char* CUiComponent_GameState::CExtraLatchedState::GetDecoratedPlayerName( int nPlayerIndex, bool bHTMLSafe )
{
	if ( m_bLatched )
	{
		if ( nPlayerIndex >= 0 && nPlayerIndex < MAX_PLAYERS )
		{
			if ( bHTMLSafe )
				return m_DecoratedHTMLPlayerNames[nPlayerIndex];
			else
				return m_DecoratedPlayerNames[nPlayerIndex];
		}
		return "";
	}

	if ( !GetCSResources() )
		return "";

	wchar_t wszName[4 * MAX_DECORATED_PLAYER_NAME_LENGTH]; // *4 for 'safe' version which escapes html entities
	wszName[0] = L'\0';
	EDecoratedPlayerNameFlag_t flags = k_EDecoratedPlayerNameFlag_Simple | k_EDecoratedPlayerNameFlag_DontShowClanName;
	if ( bHTMLSafe )
		flags |= k_EDecoratedPlayerNameFlag_HTMLEscapeString;
	else
		flags |= k_EDecoratedPlayerNameFlag_DontHTMLEscapeString;
	GetCSResources()->GetDecoratedPlayerName( nPlayerIndex, wszName, sizeof( wszName ), flags );

	// Convert to UTF-8
	static char szAPlayerNameUTF8[4 * 4 * MAX_DECORATED_PLAYER_NAME_LENGTH]; // ~12k buffer (!)
	V_UnicodeToUTF8( wszName, szAPlayerNameUTF8, sizeof( szAPlayerNameUTF8 ) );

	return szAPlayerNameUTF8;
}


char const * CUiComponent_GameState::GetPlayerName( char const* szXuid )
{
	return GetPlayerNameWithNoHTMLEscapes( szXuid );
}

char const * CUiComponent_GameState::GetPlayerNameWithNoHTMLEscapes( char const* szXuid )
{
	// Don't escape HTML entities in the player name
	int nPlayerIndex = GetPlayerIndex( szXuid );
	return GetExtraState()->GetDecoratedPlayerName( nPlayerIndex, false );
}

char const * CUiComponent_GameState::GetPlayerXuidStringFromEntIndex( int nUserId )
{
	Assert( nUserId > 0 && nUserId <= MAX_PLAYERS );

	static CFmtStr s_fmt;
	s_fmt.Format( "%llu", GetPlayerXuidFromEntIndex( nUserId ) );

	return( s_fmt.Get() );
}

char const * CUiComponent_GameState::GetPlayerXuidFromUserID( int nUserID )
{
	CBasePlayer* pPlayer = UTIL_PlayerByUserId( nUserID );
	int nPlayerIndex = 0;
	if ( pPlayer )
	{
		nPlayerIndex = pPlayer->entindex();
	}

	return GetPlayerXuidStringFromEntIndex( nPlayerIndex );
}


// returns local player's 64 bit SteamID ( "Xuid" ) OR the user index of the Suspect in the case of Overwatch.
char const * CUiComponent_GameState::GetLocalPlayerXuid()
{
	static CFmtStr s_strLocalId;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
#if defined( OSX ) && defined( __aarch64__ ) && defined( NO_STEAM )
	// As with bots and anonymous demos, local UI identity is an entity index.
	return pLocalPlayer ? GetPlayerXuidStringFromEntIndex( pLocalPlayer->entindex() ) : "";
#endif

	// overwatch suspect
	//

	if ( !m_szLocalId[0] )
	{
		if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
		{
			if ( pParameters->m_uiLockFirstPersonAccountID && pParameters->m_bAnonymousPlayerIdentity )
			{
				// compare names to find player index
				static wchar_t const * const kwszTheSuspect = g_pVGuiLocalize->Find( "#CSGO_Overwatch_TheSuspect" );
				Assert( kwszTheSuspect );

				// Convert to UTF-8
				static char szAPlayerNameUTF8[ 4 * 4 * MAX_DECORATED_PLAYER_NAME_LENGTH ]; // ~12k buffer (!)
				V_UnicodeToUTF8( kwszTheSuspect, szAPlayerNameUTF8, sizeof( szAPlayerNameUTF8 ) );

				for ( int idxPlayer = 1; idxPlayer <= gpGlobals->maxClients; idxPlayer++ )
				{
					const char * szName = GetExtraState()->GetDecoratedPlayerName( idxPlayer, false );
					if ( V_stristr( szName, szAPlayerNameUTF8 ) )
					{
						s_strLocalId.Format( "%d", idxPlayer );

						m_szLocalId = s_strLocalId.Get();
					}
				}
			}
		}

		if ( pLocalPlayer )
		{
			if ( steamapicontext->SteamUser() )
			{
				s_strLocalId.Format( "%llu", steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() );

				m_szLocalId = s_strLocalId.Get();
			}
		}

	}

	return m_szLocalId;
}



char const * CUiComponent_GameState::GetPlayerTeamName( char const* szXuid )
{
	if ( !GetGameState() || !g_PR )
		return "";

	// We don't latch team name here, is that needed? (use of g_PR)

	XUID xuid = GetXuidFromXuidString( szXuid );

	int nPlayerIndex = GetPlayerIndex( szXuid );

	if ( nPlayerIndex )
		return g_BannedWords.CensorExternalString( xuid, g_PR->GetTeamName( GetGameState()->GetTeam( nPlayerIndex ) ) );
	else
		return "BAD PLAYER INDEX";
}



int CUiComponent_GameState::GetPlayerTeamNumber( const char * szXuid )
{
	if ( !GetCSResources() )
		return TEAM_UNASSIGNED;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->GetTeam( nPlayerIndex );
}


bool CUiComponent_GameState::ArePlayersEnemies( const char * szXuid1, const char * szXuid2 )
{
	int nPlayerIndex1 = GetPlayerIndex( szXuid1 );
	int nPlayerIndex2 = GetPlayerIndex( szXuid2 );

	if ( nPlayerIndex1 == nPlayerIndex2 )
		return false;

	CCSPlayer* pPlayer1 = ToCSPlayer( UTIL_PlayerByIndex( nPlayerIndex1 ) );
	CCSPlayer* pPlayer2 = ToCSPlayer( UTIL_PlayerByIndex( nPlayerIndex2 ) );

	if ( !pPlayer1 || !pPlayer2 )
		return false;

	if ( pPlayer1->IsSpectator() || pPlayer2->IsSpectator() )
		return false;

	return pPlayer1->IsOtherEnemy( pPlayer2 );

}


int CUiComponent_GameState::GetCoachingTeamNumber( const char * szXuid )
{
	if ( !GetCSResources() )
		return TEAM_UNASSIGNED;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->GetCoachingTeam( nPlayerIndex );

}

// player or coach
int CUiComponent_GameState::GetAssociatedTeamNumber( const char * szXuid )
{
	if ( !GetCSResources() )
		return TEAM_UNASSIGNED;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->GetCoachingTeam( nPlayerIndex ) ? GetGameState()->GetCoachingTeam( nPlayerIndex ) : GetGameState()->GetTeam( nPlayerIndex );
}


bool CUiComponent_GameState::IsPlayerConnected( const char * szXuid )
{
	if ( !GetGameState() )
		return false;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return ( GetGameState()->IsConnected( nPlayerIndex ) && !( GetExtraState()->IsHLTV( nPlayerIndex ) ) );
}

bool CUiComponent_GameState::IsPlayerAlive( const char * szXuid )
{
	if ( !GetGameState() )
		return false;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->IsAlive( nPlayerIndex );
}


int CUiComponent_GameState::GetPlayerPing( char const* szXuid )
{
	if ( !GetGameState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->GetPing( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerKills( char const* szXuid )
{
	if ( !GetGameState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->GetKills( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerAssists( char const* szXuid )
{
	if ( !GetGameState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->GetAssists( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerDeaths( char const* szXuid )
{
	if ( !GetGameState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetGameState()->GetDeaths( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerMVPs( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetCSState()->GetNumMVPs( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerGungameLevel( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetCSState()->GetGunGameLevel( nPlayerIndex );
}


bool CUiComponent_GameState::IsFakePlayer( char const* szXuid )
{
	Assert( GetGameState() ); // Too early in the loading process to be calling this, investigate
	if ( !GetGameState() )
		return false;

	int nPlayerIndex = GetPlayerIndex( szXuid );
	return GetExtraState()->IsFakePlayer( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerMoney( char const* szXuid )
{
	if ( !GetGameState() )
		return 0;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetExtraState()->GetPlayerMoney( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerScore( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetCSState()->GetScore( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerLifetime( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	if ( CSGameRules()->IsWarmupPeriod() )
	{
		return 0;
	}
	else
	{
		int nPlayerIndex = GetPlayerIndex( szXuid );
		return GetCSState()->GetLifetime( nPlayerIndex );
	}
}

float CUiComponent_GameState::GetSurvivalTimeUntilNextWave( void )
{
	if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() && GetDangerZoneController() &&
		CSGameRules()->GetSurvivalRules() && CSGameRules()->GetSurvivalRules()->GetSurvivalStartTime() && gpGlobals->curtime > CSGameRules()->GetSurvivalRules()->GetSurvivalStartTime() + 1.0f )
	{
		return GetDangerZoneController()->GetTimeUntilNextWave();
	}

	return 0.f;
}

float CUiComponent_GameState::GetPlayerVoiceVolume( char const * szXuid )
{
	XUID xuid = GetXuidFromXuidString( szXuid );

	if ( xuid == 0 )
		return -1;

	
	return ( engine->GetPlayerVoiceVolume( xuid ) );
}

void CUiComponent_GameState::SetPlayerVoiceVolume( char const* szXuid, float val )
{
	XUID xuid = GetXuidFromXuidString( szXuid );

	if ( xuid == 0 )
		return;


	engine->SetPlayerVoiceVolume( xuid, val );
}

bool CUiComponent_GameState::IsDemoOrHltv()
{
	return ( engine->IsPlayingDemo() || g_bEngineIsHLTV );
}

bool CUiComponent_GameState::IsLocalPlayerHLTV()
{
	bool bQ = false;
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer && pPlayer->IsHLTV() )
	{
		bQ = true;
	}

	return bQ;
}

bool CUiComponent_GameState::IsHLTVAutodirectorOn()
{
	return ( g_bEngineIsHLTV && HLTVCamera()->AutoDirectorState() == C_HLTVCamera::AUTODIRECTOR_ON );
}

void CUiComponent_GameState::SetCasterIsCameraman( int nAccountID )
{
	ConVarRef spec_autodirector_cameraman( "spec_autodirector_cameraman" );

	//C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	//CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
	if ( nAccountID == 0 )
	{
		// never set spec_autodirector_cameraman to 0, just disable the autodirector convar
		HLTVCamera()->SetAutoDirector( C_HLTVCamera::AUTODIRECTOR_OFF );
		//spec_autodirector.SetValue( false );
		//hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Camera_Off" ) );
	}
	else
	{
		spec_autodirector_cameraman.SetValue( nAccountID );
		HLTVCamera()->SetAutoDirector( C_HLTVCamera::AUTODIRECTOR_ON );
		//spec_autodirector.SetValue( true );
		//hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Camera_On" ) );
	}
}

void CUiComponent_GameState::SetCasterIsHeard( int nAccountID )
{
	ConVarRef voice_caster_enable( "voice_caster_enable" );
	voice_caster_enable.SetValue( nAccountID );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
	if ( nAccountID == 0 )
		hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Voice_Off" ) );
	else
		hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Voice_On" ) );
}

void CUiComponent_GameState::SetCasterControlsXray( int nAccountID )
{
	ConVarRef spec_cameraman_xray( "spec_cameraman_xray" );
	spec_cameraman_xray.SetValue( nAccountID );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
	if ( nAccountID == 0 )
		hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Xray_Off" ) );
	else
		hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Xray_On" ) );
}

void CUiComponent_GameState::SetCasterControlsUI( int nAccountID )
{
	ConVarRef spec_cameraman_ui( "spec_cameraman_ui" );
	spec_cameraman_ui.SetValue( nAccountID );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
	if ( nAccountID == 0 )
		hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_UI_Off" ) );
	else
		hudChat->ChatPrintfW( pPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_UI_On" ) );
}



bool CUiComponent_GameState::IsLocalPlayerPlayingMatch( void )
{
	return engine->IsInGame() && engine->IsConnected() && !IsDemoOrHltv();
}

bool CUiComponent_GameState::AreTeamsPlayingSwitchedSides( void )
{
	if ( !CSGameRules() )
		return false;

	return CSGameRules()->AreTeamsPlayingSwitchedSides();
}

bool CUiComponent_GameState::AreTeamsPlayingSwitchedSidesInRound( int nRound )
{
	if ( !CSGameRules() )
		return false;

	return CSGameRules()->AreTeamsPlayingSwitchedSidesInRound( nRound );
}


bool CUiComponent_GameState::IsSelectedPlayerMuted( char const* szXuid )
{
	// UNLATCHED
	if ( !GetCSResources() )
		return false;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetClientVoiceMgr() && GetClientVoiceMgr()->IsPlayerBlocked( nPlayerIndex );
}

void CUiComponent_GameState::ToggleMute( char const* szXuid )
{
	if ( !GetGameState() )
		return;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	if ( GetClientVoiceMgr() && !GetExtraState()->IsFakePlayer( nPlayerIndex ) && nPlayerIndex != GetLocalPlayerIndex() )
	{
		GetClientVoiceMgr()->SetPlayerBlockedState( nPlayerIndex );
	}
}

bool CUiComponent_GameState::IsReportCategoryEnabledForSelectedPlayer( char const* szXuid, const char * szCategory )
{
	if ( !GetGameState() )
		return false;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	bool bEnabled = true;
	if ( !Q_stricmp( szCategory, "grief" ) ||
		!Q_stricmp( szCategory, "wallhack" ) ||
		!Q_stricmp( szCategory, "speedhack" ) ||
		!Q_stricmp( szCategory, "aimbot" ) )
	{	// We can only report human players besides ourself who are on a playing team
		bEnabled = ( !GetExtraState()->IsFakePlayer( nPlayerIndex ) && ( nPlayerIndex != GetLocalPlayerIndex() ) &&
			( ( GetGameState()->GetTeam( nPlayerIndex ) == TEAM_TERRORIST ) || ( GetGameState()->GetTeam( nPlayerIndex ) == TEAM_CT ) ) );
	}
	else
	{
		bEnabled = ( nPlayerIndex != GetLocalPlayerIndex() );
	}

	return bEnabled;
}


void CUiComponent_GameState::SubmitPlayerReport( char const* szXuid, const char * szCategory )
{
	CSteamID steamId( ( uint64 )Q_atoi64( szXuid ) );

	if ( !steamId.IsValid() || !steamId.BIndividualAccount() )
		return;

// 	GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientReportPlayer > msg( k_EMsgGCCStrike15_v2_ClientReportPlayer );
// 	msg.Body().set_account_id( steamId.GetAccountID() );
// 
// 	extern uint64 Helper_GetLastCompetitiveMatchId();
// 	msg.Body().set_match_id( Helper_GetLastCompetitiveMatchId() );
// 
// 	//
// 	// Ensure that the flags that we already reported the player for persist again
// 	//
// 	msg.Body().set_rpt_textabuse( !!Q_stristr( szCategory, "textabuse" ) );
// 	msg.Body().set_rpt_voiceabuse( !!Q_stristr( szCategory, "voiceabuse" ) );
// 	msg.Body().set_rpt_teamharm( !!Q_stristr( szCategory, "grief" ) );
// 	msg.Body().set_rpt_speedhack( !!Q_stristr( szCategory, "speedhack" ) );
// 	msg.Body().set_rpt_wallhack( !!Q_stristr( szCategory, "wallhack" ) );
// 	msg.Body().set_rpt_aimbot( !!Q_stristr( szCategory, "aimbot" ) );
// 	GCClientSystem()->GetGCClient()->BSendMessage( msg );
}

void CUiComponent_GameState::SubmitServerReport( const char * szCategory )
{
// 	GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientReportServer > msg( k_EMsgGCCStrike15_v2_ClientReportServer );
// 
// 	extern uint64 Helper_GetLastCompetitiveMatchId();
// 	msg.Body().set_match_id( Helper_GetLastCompetitiveMatchId() );
// 
// 	msg.Body().set_rpt_poorperf( !!Q_stristr( szCategory, "perf" ) );
// 	msg.Body().set_rpt_abusivemodels( !!Q_stristr( szCategory, "models" ) );
// 	msg.Body().set_rpt_badmotd( !!Q_stristr( szCategory, "motd" ) );
// 	msg.Body().set_rpt_listingabuse( !!Q_stristr( szCategory, "listing" ) );
// 	msg.Body( ).set_rpt_inventoryabuse( !!Q_stristr( szCategory, "inventory" ) );
// 	
// 	GCClientSystem()->GetGCClient()->BSendMessage( msg );
}


///////////
///////////
/////////// COMMENDATIONS
///////////
///////////
///////////

static double s_timeLastCommendationsUpdate = 0;
static uint32 s_numCommendationTokensAvailable = 0;
static CUtlMap< XUID, CMsgGCCStrike15_v2_ClientCommendPlayer > s_mapSubmittedCommendations;

#ifndef NO_STEAM
class ClientJob_EMsgGCCStrike15_v2_ClientCommendPlayerQueryResponse : public GCSDK::CGCClientJob
{
public:
	explicit ClientJob_EMsgGCCStrike15_v2_ClientCommendPlayerQueryResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
	{
	}

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_ClientCommendPlayer> msg( pNetPacket );

		DevMsg( "Commendation information (%u) for player %u [%u-%u-%u]\n",
			msg.Body().tokens(), msg.Body().account_id(),
			( msg.Body().has_commendation() && msg.Body().commendation().cmd_friendly() ),
			( msg.Body().has_commendation() && msg.Body().commendation().cmd_teaching() ),
			( msg.Body().has_commendation() && msg.Body().commendation().cmd_leader() ) );

		if ( !s_mapSubmittedCommendations.Count() )
			SetDefLessFunc( s_mapSubmittedCommendations );

		CSteamID steamid( msg.Body().account_id(), steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
		if ( msg.Body().has_commendation() )
			s_mapSubmittedCommendations.InsertOrReplace( steamid.ConvertToUint64(), msg.Body() );

		if ( msg.Body().has_tokens() )
		{
			s_numCommendationTokensAvailable = msg.Body().tokens();
			s_timeLastCommendationsUpdate = Plat_FloatTime();
		}

		panorama::DispatchEvent( GameState_CommendPlayerQueryResponse(), NULL );

		return true;
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_ClientCommendPlayerQueryResponse, k_EMsgGCCStrike15_v2_ClientCommendPlayerQueryResponse );
#endif




///////////
///////////
/////////// REPORTS
///////////
///////////
///////////
#if defined ( PANORAMA_ENABLE )
#ifndef NO_STEAM
class ClientJob_EMsgGCCStrike15_v2_ClientReportResponse : public GCSDK::CGCClientJob
{
public:
	explicit ClientJob_EMsgGCCStrike15_v2_ClientReportResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
	{
	}

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_ClientReportResponse> msg( pNetPacket );

		Msg( "Confirmation number %llu for %u (%u/%u) (%u tokens)\n",
			msg.Body().confirmation_id(),
			msg.Body().account_id(),
			msg.Body().response_type(), msg.Body().response_result(), msg.Body().tokens() );

		if ( msg.Body().has_tokens() )
		{
			s_numCommendationTokensAvailable = msg.Body().tokens();
			s_timeLastCommendationsUpdate = Plat_FloatTime();
		}

		char const *szPersonaName = NULL;
		if ( msg.Body().account_id() )
			szPersonaName = Helper_GetFriendPersonaNameSanitized(
				CSteamID( msg.Body().account_id(), steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual ) );
		// 			else if ( msg.Body().server_ip() )
		// 				szPersonaName = "Game";
		if ( !szPersonaName || !*szPersonaName )
			szPersonaName = "Player";

		char const *szFormatString = NULL;
		switch ( msg.Body().response_type() )
		{
		case k_EMsgGCCStrike15_v2_ClientReportServer:
			szPersonaName = "Server";
			szFormatString = msg.Body().response_result() ? "#SFUI_Notice_Report_Server_Success" : "#SFUI_Notice_Report_Server_Failed";
			break;
		case k_EMsgGCCStrike15_v2_ClientReportPlayer:
			szFormatString = msg.Body().response_result() ? "#SFUI_Notice_Report_Player_Success" : "#SFUI_Notice_Report_Player_Failed";
			break;
		case k_EMsgGCCStrike15_v2_ClientCommendPlayer:
			switch ( msg.Body().response_result() )
			{
			case 1:
				szFormatString = "#SFUI_Notice_Commend_Player_Success";
				break;
			case 2:
				szFormatString = "#SFUI_Notice_Commend_Player_TooEarly";
				break;
			default:
				szFormatString = "#SFUI_Notice_Commend_Player_Failed";
				break;
			}
			break;
		}
		if ( !szFormatString )
			return false;

		wchar_t wchPlayerName[ 128 ] = { 0 };
		g_pVGuiLocalize->ConvertANSIToUnicode( szPersonaName, wchPlayerName, sizeof( wchPlayerName ) );

		wchar_t wchConfirmationNumber[ 64 ] = { 0 };
		V_snwprintf( wchConfirmationNumber, Q_ARRAYSIZE( wchConfirmationNumber ), L"%llu", msg.Body().confirmation_id() );

		wchar_t wchOutputBuf[ 128 ] = { 0 };
		if ( const wchar_t *pBuf = g_pVGuiLocalize->Find( szFormatString ) )
		{
			g_BannedWords.CensorBannedWordsInplace( wchPlayerName );
			g_pVGuiLocalize->ConstructString( wchOutputBuf, sizeof( wchOutputBuf ), pBuf, 2, wchPlayerName, wchConfirmationNumber );

			if ( CHudElement *pElement = GetHud().FindElement( "CCSGO_HudVoiceStatus" ) )
			{
				char chNotice[ 1024 ];
				V_UnicodeToUTF8( ( wchar_t * ) wchOutputBuf, chNotice, sizeof( chNotice ) );
				chNotice[ sizeof( chNotice ) - 1 ] = 0;
				( ( CCSGO_HudVoiceStatus * ) pElement )->PushNotice( chNotice, -1 );
			}
		}

		return true;
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_ClientReportResponse, k_EMsgGCCStrike15_v2_ClientReportResponse );
#endif
#endif


bool CUiComponent_GameState::QueryServersForCommendation( char const* szXuid )
{
	CSteamID steamId( ( uint64 )Q_atoi64( szXuid ) );
	if ( !steamId.IsValid() || !steamId.BIndividualAccount() )
		return true;

	if ( s_mapSubmittedCommendations.Count() &&
		( s_mapSubmittedCommendations.Find( GetXuidFromXuidString( szXuid ) ) != s_mapSubmittedCommendations.InvalidIndex() ) &&
		s_timeLastCommendationsUpdate && ( Plat_FloatTime() - s_timeLastCommendationsUpdate < 30 * 60 ) ) // refresh every 30 minutes regardless of cache state
	{
		// We already know about that player
		return true;
	}
	else
	{
		// We are going to query the servers
// 		GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientCommendPlayer > msg( k_EMsgGCCStrike15_v2_ClientCommendPlayerQuery );
// 		msg.Body().set_account_id( steamId.GetAccountID() );
// 		GCClientSystem()->GetGCClient()->BSendMessage( msg );

		// Reply to panorama that we successfully asked
		return false;
	}
}


int CUiComponent_GameState::GetCommendationTokensAvailable( )
{
	return ( int )s_numCommendationTokensAvailable;
}

void CUiComponent_GameState::GetMyCommendationsJSOForUser( const v8::FunctionCallbackInfo<v8::Value>& args )
{

	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );
	v8::Local< v8::Object > pObject = v8::Object::New( args.GetIsolate() );

	v8::String::Utf8Value xuid(v8::Isolate::GetCurrent(),  args[ 0 ] );

	if ( GetXuidFromXuidString( *xuid ) == 0 )
		return;

	static CFmtStr fmtStr;
	fmtStr.Clear();
	if ( s_mapSubmittedCommendations.Count() &&
		( s_mapSubmittedCommendations.Find( GetXuidFromXuidString( *xuid ) ) != s_mapSubmittedCommendations.InvalidIndex() ) )
	{
		CreateJSOEntry_Bool( pObject, "valid", true );

		CMsgGCCStrike15_v2_ClientCommendPlayer &msg = s_mapSubmittedCommendations.Element( s_mapSubmittedCommendations.Find( GetXuidFromXuidString( *xuid ) ) );
		if ( msg.has_commendation() && msg.commendation().cmd_friendly() )
		{
			CreateJSOEntry_Bool( pObject, "friendly", true );
		}
		if ( msg.has_commendation() && msg.commendation().cmd_teaching() )
		{
			CreateJSOEntry_Bool( pObject, "teaching", true );
		}
		if ( msg.has_commendation() && msg.commendation().cmd_leader() )
		{
			CreateJSOEntry_Bool( pObject, "leader", true );
		}
	}
	else
	{
		CreateJSOEntry_Bool( pObject, "valid", false );
	}

	args.GetReturnValue().Set( pObject );
}

void CUiComponent_GameState::SubmitCommendation( char const* szXuid, const char * szCommendations )
{
	CSteamID steamId( ( uint64 )Q_atoi64( szXuid ) );

	if ( !steamId.IsValid() || !steamId.BIndividualAccount() )
		return;


	GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientCommendPlayer > msg( k_EMsgGCCStrike15_v2_ClientCommendPlayer );
	msg.Body().set_account_id( steamId.GetAccountID() );

// 	extern uint64 Helper_GetLastCompetitiveMatchId();
// 	msg.Body().set_match_id( Helper_GetLastCompetitiveMatchId() );

	msg.Body().mutable_commendation()->set_cmd_friendly( !!Q_stristr( szCommendations, "friendly" ) );
	msg.Body().mutable_commendation()->set_cmd_teaching( !!Q_stristr( szCommendations, "teaching" ) );
	msg.Body().mutable_commendation()->set_cmd_leader( !!Q_stristr( szCommendations, "leader" ) );

	// Store the commendation that we are submitting
	if ( !s_mapSubmittedCommendations.Count() )
		SetDefLessFunc( s_mapSubmittedCommendations );
	s_mapSubmittedCommendations.InsertOrReplace( GetXuidFromXuidString( szXuid ), msg.Body() );

//	GCClientSystem()->GetGCClient()->BSendMessage( msg );
}




char const * CUiComponent_GameState::GetPlayerClanTag( char const* szXuid )
{
	if ( !GetCSState() )
		return "";

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetCSState()->GetClanTag( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerActiveCoinRank( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetCSState()->GetActiveCoinRank( nPlayerIndex );
}

int CUiComponent_GameState::GetPlayerCompetitiveRanking( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );
	bool bCompetitiveInformationVisible = false;

	XUID xuid = ConvertToUint64( szXuid );
	if ( m_bServerRankRevealAll )
	{
		bCompetitiveInformationVisible = true; // End of match reveal
	}
	if ( steamapicontext->SteamUser() && steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() == xuid )
	{
		bCompetitiveInformationVisible = true;	// Ourselves
	}
	else if ( ( xuid != INVALID_XUID ) && steamapicontext->SteamFriends() && ( steamapicontext->SteamFriends()->GetFriendRelationship( xuid ) == k_EFriendRelationshipFriend ) )
	{
		bCompetitiveInformationVisible = true;	// We are friends
	}
	else if ( GetCSState()->GetClanTag( nPlayerIndex ) && *GetCSState()->GetClanTag( nPlayerIndex ) &&
		!V_strcmp( GetCSState()->GetClanTag( nPlayerIndex ), GetCSState()->GetClanTag( GetLocalPlayerIndex() ) ) )
	{
		bCompetitiveInformationVisible = true;	// We are playing with the same non-empty clan tag
	}
	else if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() && g_pMatchFramework && g_pMatchFramework->GetMatchSession() &&
		SessionMembersFindPlayer( g_pMatchFramework->GetMatchSession()->GetSessionSettings(), xuid ) )
	{
		bCompetitiveInformationVisible = true;	// We are playing together as a party
	}

	if ( bCompetitiveInformationVisible )
		return GetCSState()->GetCompetitiveRanking( nPlayerIndex );
	else
		return -1;

}

int CUiComponent_GameState::GetPlayerXpLevel( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetCSState()->GetPersonaDataPublicLevel( nPlayerIndex );
}


int CUiComponent_GameState::GetPlayerCompetitiveWins( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	return GetCSState()->GetCompetitiveWins( nPlayerIndex );

}


char const * CUiComponent_GameState::GetPlayerColor( char const* szXuid )
{
	if ( !GetCSState() )
		return "";

	int nPlayerIndex = GetPlayerIndex( szXuid );

	int r = 0;
	int g = 0;
	int b = 0;

	int nPlayerIdxForColor = -1;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	int nTeam = GetGameState()->GetTeam( nPlayerIndex );

	if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( nTeam ) )
	{
		nPlayerIdxForColor = GetCSState()->GetCompTeammateColor( nPlayerIndex );
	}

	switch ( nPlayerIdxForColor )
	{
		case 0:
		{
			static ConVarRef cl_teammate_color_1( "cl_teammate_color_1" );
			sscanf( cl_teammate_color_1.GetString(), "%i %i %i", &r, &g, &b );
			break;
		}
		case 1:
		{
			static ConVarRef cl_teammate_color_2( "cl_teammate_color_2" );
			sscanf( cl_teammate_color_2.GetString(), "%i %i %i", &r, &g, &b );
			break;
		}
		case 2:
		{
			static ConVarRef cl_teammate_color_3( "cl_teammate_color_3" );
			sscanf( cl_teammate_color_3.GetString(), "%i %i %i", &r, &g, &b );
			break;
		}
		case 3:
		{
			static ConVarRef cl_teammate_color_4( "cl_teammate_color_4" );
			sscanf( cl_teammate_color_4.GetString(), "%i %i %i", &r, &g, &b );
			break;
		}
		case 4:
		{
			static ConVarRef cl_teammate_color_5( "cl_teammate_color_5" );
			sscanf( cl_teammate_color_5.GetString(), "%i %i %i", &r, &g, &b );
			break;
		}

		default:
		{
			r = g = b = -1;
			break;
		}
	}


	static char szColor[ 8 ];

	if ( r == -1 )
	{
		return "";
	}
	else
	{
		V_sprintf_safe( szColor, "#%02X%02X%02X", uint32( uint8( r ) ), uint32( uint8( g ) ), uint32( uint8( b ) ) );
		return szColor;
	}



}



char const * CUiComponent_GameState::GetPlayerDisplayFlairItem( char const* szXuid )
{
// 	XUID xuid = ConvertToUint64( szXuid );
// 
// 	extern const CEconItemView * Helper_FlairItem_GetFlairIDForPlayer( XUID xuid, itemid_t *pullFauxItemID = NULL );
// 	itemid_t ullDisplayFlairItemID = 0;
// 	if ( const CEconItemView *pCoin = xuid ? Helper_FlairItem_GetFlairIDForPlayer( xuid, &ullDisplayFlairItemID ) : NULL )
// 	{
// 		static CFmtStr s_fmt;
// 		s_fmt.Format( "%llu", ullDisplayFlairItemID );
// 
// 		return( s_fmt.Get() );
// 
// 	}

	return "";

}

int CUiComponent_GameState::GetPlayerStatus( char const* szXuid )
{
	if ( !GetCSState() )
		return( None );

	int nPlayerIndex = GetPlayerIndex( szXuid );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	bool bShowExtraInfo = pLocalPlayer && (
		( pLocalPlayer->GetTeamNumber() == TEAM_UNASSIGNED ) || // we're not spawned yet
		( pLocalPlayer->IsSpectator() ) || // we are a spectator
		( pLocalPlayer->GetAssociatedTeamNumber() == GetGameState()->GetTeam( nPlayerIndex ) ) // we're on the same team
		);

	int controlledBy = GetCSState()->GetControlledByPlayer( nPlayerIndex );
	int controlling = GetCSState()->GetControlledPlayer( nPlayerIndex );
	bool alive = controlledBy || ( GetGameState()->IsAlive( nPlayerIndex ) && !controlling );
	bool bChangingTeams = false;

	if ( GetGameState()->GetPendingTeam( nPlayerIndex ) != GetGameState()->GetTeam( nPlayerIndex ) || ( CSGameRules() && CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME ) )
	{
		bChangingTeams = true;

		if ( !alive && GetGameState()->GetTeam( nPlayerIndex ) > TEAM_SPECTATOR )
			return ( SwitchTeamsDead );
		else
			return( SwitchTeams );
	}
	else if ( !alive && GetGameState()->GetTeam( nPlayerIndex ) > TEAM_SPECTATOR )
	{
		return( Dead );
	}
	else if ( bShowExtraInfo && ( GetCSState()->HasC4( nPlayerIndex ) || ( controlledBy != 0 && GetCSState()->HasC4( controlledBy ) ) ) )
	{
		return( Bomb );
	}
	else if ( bShowExtraInfo && ( GetCSState()->HasDefuser( nPlayerIndex ) || ( controlledBy != 0 && GetCSState()->HasDefuser( controlledBy ) ) ) )
	{
		return( Defuser );
	}
	else
	{
		return( None );
	}

	if ( !bChangingTeams )
	{
		//Set the dominated icon
		if ( pLocalPlayer->IsPlayerDominated( nPlayerIndex ) )
		{
			if ( alive )
			{
				return( Dominated );
			}
			else
			{
				return( DominatedDead );
			}
		}
		else if ( pLocalPlayer->IsPlayerDominatingMe( nPlayerIndex ) )
		{
			if ( alive )
			{
				return( Nemesis );
			}
			else
			{
				return( NemesisDead );
			}
		}
	}

}


const char * CUiComponent_GameState::GetPlayerModel( char const* szXuid )
{
	// TODO: Do we need to latch this?
	if ( !GetCSResources() )
		return "";

	int nPlayerIndex = GetPlayerIndex( szXuid );

	C_BasePlayer * pPlayer = UTIL_PlayerByIndex( nPlayerIndex );

	if ( pPlayer && pPlayer->GetModelPtr() )
	{
		return  pPlayer->GetModelPtr()->pszName();
	}
	else
	{
		return "";
	}
}

char const *  CUiComponent_GameState::GetPlayerActiveWeaponItemId( char const * szXuid )
{
	// TODO: Do we need to latch this?
	if ( !GetCSResources() )
		return "";

	int nPlayerIndex = GetPlayerIndex( szXuid );

	C_CSPlayer * pPlayer = ToCSPlayer( UTIL_PlayerByIndex( nPlayerIndex ) );

	if ( !pPlayer )
		return "";

	C_WeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
	if ( !pWeapon )
		return "";

	CEconItemView *pItem = pWeapon->GetEconItemView();
	if ( !pItem )
		return "";

	static CFmtStr s_fmtResult;
	s_fmtResult.Format( "%llu", pItem->GetItemID() ? pItem->GetItemID() : pItem->GetFauxItemIDFromDefinitionIndex() );
	return s_fmtResult.Access();
}


// commends

int CUiComponent_GameState::GetPlayerCommendsLeader( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bCommendsVisible = pLocalPlayer && !g_bEngineIsHLTV && ( pLocalPlayer->GetTeamNumber() == GetGameState()->GetTeam( nPlayerIndex ) );
	return bCommendsVisible ? GetCSState()->GetPersonaDataPublicCommendsLeader( nPlayerIndex ) : 0;
}

int CUiComponent_GameState::GetPlayerCommendsTeacher( char const* szXuid )
{
	if ( !GetCSState() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bCommendsVisible = pLocalPlayer && !g_bEngineIsHLTV && ( pLocalPlayer->GetTeamNumber() == GetGameState()->GetTeam( nPlayerIndex ) );
	return bCommendsVisible ? GetCSState()->GetPersonaDataPublicCommendsTeacher( nPlayerIndex ) : 0;
}

int CUiComponent_GameState::GetPlayerCommendsFriendly( char const* szXuid )
{
	if ( !GetCSResources() )
		return -1;

	int nPlayerIndex = GetPlayerIndex( szXuid );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bCommendsVisible = pLocalPlayer && !g_bEngineIsHLTV && ( pLocalPlayer->GetTeamNumber() == GetGameState()->GetTeam( nPlayerIndex ) );
	return bCommendsVisible ? GetCSState()->GetPersonaDataPublicCommendsFriendly( nPlayerIndex ) : 0;
}







extern ConVar mp_maxrounds;
extern ConVar mp_overtime_maxrounds;
extern ConVar nextlevel;

#if defined ( PANORAMA_ENABLE )


// a javascript-exposed method that returns a JSO
//
void CUiComponent_GameState::GetTimeDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( !CSGameRules() )
		return;

	if ( !GetCSState() )
		return;

	// TODO: No latched data here, is that ok?

	int numRoundsToEndMatch = mp_maxrounds.GetInt() + ( CSGameRules()->GetOvertimePlaying() * mp_overtime_maxrounds.GetInt() );

	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );
	v8::Local< v8::Object > pObject = v8::Object::New( args.GetIsolate() );

	CreateJSOEntry_Number( pObject, "gamephase", GetGamePhase() );

	extern ConVar mp_halftime;

	CreateJSOEntry_Bool( pObject, "has_halftime", mp_halftime.GetBool() );

	CreateJSOEntry_Number( pObject, "maxrounds", mp_maxrounds.GetInt() );
	CreateJSOEntry_Number( pObject, "maxrounds_overtime", mp_overtime_maxrounds.GetInt() );

	CreateJSOEntry_Number( pObject, "maxrounds_this_period", CSGameRules()->GetOvertimePlaying() ? mp_overtime_maxrounds.GetInt() : mp_maxrounds.GetInt() );

	CreateJSOEntry_Number( pObject, "first_round_this_period", CSGameRules()->GetOvertimePlaying() ? mp_maxrounds.GetInt() + (( CSGameRules()->GetOvertimePlaying() - 1 ) * mp_overtime_maxrounds.GetInt() ) + 1 : 1 );
	CreateJSOEntry_Number( pObject, "last_round_this_period", CSGameRules()->GetOvertimePlaying() ? mp_maxrounds.GetInt() + CSGameRules()->GetOvertimePlaying() * mp_overtime_maxrounds.GetInt() : mp_maxrounds.GetInt() );

	CreateJSOEntry_Number( pObject, "overtime", CSGameRules()->GetOvertimePlaying() );

	CreateJSOEntry_Number( pObject, "roundtime", CSGameRules()->GetRoundLength() );
	CreateJSOEntry_Number( pObject, "maptime", mp_timelimit.GetInt() );
	CreateJSOEntry_Number( pObject, "roundtime_remaining", CSGameRules()->GetRoundRemainingTime() );
	CreateJSOEntry_Number( pObject, "roundtime_elapsed", CSGameRules()->GetRoundElapsedTime() );
	CreateJSOEntry_Number( pObject, "maptime_remaining", CSGameRules()->GetMapRemainingTime() );
	CreateJSOEntry_Number( pObject, "maptime_elapsed", CSGameRules()->GetMapElapsedTime() );
	CreateJSOEntry_Number( pObject, "rounds_remaining", numRoundsToEndMatch - CSGameRules()->GetTotalRoundsPlayed() );
	CreateJSOEntry_Number( pObject, "rounds_played", CSGameRules()->GetTotalRoundsPlayed() );
	CreateJSOEntry_Number( pObject, "num_wins_to_clinch", CSGameRules()->GetNumWinsToClinch() );
	CreateJSOEntry_Number( pObject, "num_wins_to_clinch_this_period", CSGameRules()->GetOvertimePlaying() ? mp_overtime_maxrounds.GetInt() / 2 + 1 : mp_maxrounds.GetInt() / 2 + 1);


	bool MatchCanClinch();

	CreateJSOEntry_Number( pObject, "can_clinch", MatchCanClinch() );


	if ( CSGameRules()->IsWarmupPeriod() )
	{
		if ( CSGameRules()->IsWarmupPeriodPaused() )
		{
			CreateJSOEntry_Bool( pObject, "hide", true );
		}
		else
		{
			int nWarmupTimeRemaining = ( int )CSGameRules()->GetWarmupPeriodEndTime() - ( int )gpGlobals->curtime;
			CreateJSOEntry_Number( pObject, "time", nWarmupTimeRemaining );
		}
	}
	else if ( CSGameRules()->IsQueuedMatchmaking() && m_flTimeWhenShudownHappens && ( Plat_FloatTime() < m_flTimeWhenShudownHappens ) )
	{
		CreateJSOEntry_Number( pObject, "time", ( int )( m_flTimeWhenShudownHappens - Plat_FloatTime() ) );
	}
	else
	{
		CreateJSOEntry_Number( pObject, "time", ( int )CSGameRules()->GetTimeUntilNextPhaseStarts() );
	}


	args.GetReturnValue().Set( pObject );

}

void CUiComponent_GameState::GetMatchEndWinDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( !CSGameRules() )
		return;

	C_Team *pTeamTerrorist = GetGlobalTeam( TEAM_TERRORIST );
	Assert( pTeamTerrorist );
	C_Team *pTeamCT = GetGlobalTeam( TEAM_CT );
	Assert( pTeamCT );

	int nTScore = pTeamTerrorist ? pTeamTerrorist->Get_Score() : 0;
	int nCTScore = pTeamCT ? pTeamCT->Get_Score() : 0;

	const char * szString = "";

	int nWinningTeam = 0;
	int nLosingTeam = 0;

	XUID xuidWinningPlayer = 0;

	bool bShowWinBanner = true;

	if ( ( pTeamCT && pTeamCT->m_bSurrendered ) || ( pTeamTerrorist && pTeamTerrorist->m_bSurrendered ) )
	{
		szString = "#Scoreboard_Final_Surrendered";

		if ( pTeamCT->m_bSurrendered )
		{
			nWinningTeam = TEAM_TERRORIST;
			nLosingTeam = TEAM_CT;
		}
		else
		{
			nWinningTeam = TEAM_CT;
			nLosingTeam = TEAM_TERRORIST;
		}

		szString = "#Scoreboard_Final_Won";
	}
	else if ( m_iFreeForAllModeWinnerIndex > 0 )
	{
		if ( GetGameState() )
		{
			szString = "#Scoreboard_GG_The_Winner";
			xuidWinningPlayer = GetPlayerXuidFromEntIndex( m_iFreeForAllModeWinnerIndex );
			nWinningTeam = GetGameState()->GetTeam( m_iFreeForAllModeWinnerIndex );
		}
	}
	else if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
	{
		nWinningTeam = CSGameRules()->m_iRoundWinStatus;
		szString = "#SFUI_Scoreboard_Final_Won";
	}
	else if ( nTScore == nCTScore )
	{
		szString = "#Scoreboard_Final_Tie";

		// It's a zero scoring tie. Assume the match was aborted. Don't confuse the players with a win banner.
		if ( nTScore == 0 )
		{
			bShowWinBanner = false;
		}
	}
	else
	{
		if ( nTScore > nCTScore )
		{
			nWinningTeam = TEAM_TERRORIST;
			nLosingTeam = TEAM_CT;
		}
		else
		{
			nWinningTeam = TEAM_CT;
			nLosingTeam = TEAM_TERRORIST;
		}

		szString = "#Scoreboard_Final_Won";
	}

	if ( bShowWinBanner )
	{
		v8::Isolate::Scope isolate_scope( args.GetIsolate() );
		v8::HandleScope handle_scope( args.GetIsolate() );
		v8::Local< v8::Object > pObject = v8::Object::New( args.GetIsolate() );

		CreateJSOEntry_String( pObject, "text", szString );
		CreateJSOEntry_Number( pObject, "winning_team_number", nWinningTeam );
		CreateJSOEntry_Number( pObject, "losing_team_number", nLosingTeam );
		CreateJSOEntry_String( pObject, "winning_player", CFmtStr( "%llu", xuidWinningPlayer ).Get() );

		args.GetReturnValue().Set( pObject );
	}
	// else return value is 'undefined'
}



void CUiComponent_GameState::GetScoreDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( !CSGameRules() )
		return;

	// TODO: we aren't latching any team data here, but that's probably ok

	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );
	v8::Local< v8::Object > pObject = v8::Object::New( args.GetIsolate() );

	v8::Local< v8::Object > pTeamDataObject = CreateJSOSubObject( pObject, "teamdata" );


	for ( int iTeam = 0; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		C_Team *pTeam = GetGlobalTeam( iTeam );

		v8::Local< v8::Object > pNewTeamObject = CreateJSOSubObject( pTeamDataObject, pTeam->Get_Name() );

		CreateJSOEntry_String( pNewTeamObject, "team_name", pTeam->Get_Name() );
		CreateJSOEntry_Number( pNewTeamObject, "team_number", iTeam );
		CreateJSOEntry_Number( pNewTeamObject, "clan_id", pTeam->GetClanID() );

		// use the GetTeamClanName defined here (rather than pTeam->GetClanName())
		// in order to fallback to "TERRORISTS" and "COUNTER-TERRORISTS"
		//
		CreateJSOEntry_String( pNewTeamObject, "clan_name", GetTeamClanName( pTeam->Get_Name() ) );

		CreateJSOEntry_String( pNewTeamObject, "flag", pTeam->Get_FlagImageString() );
		CreateJSOEntry_String( pNewTeamObject, "logo", pTeam->Get_LogoImageString() );

		CreateJSOEntry_Number( pNewTeamObject, "score", pTeam->Get_Score() );

		if ( CSGameRules()->HasHalfTime() )
		{
			CreateJSOEntry_Number( pNewTeamObject, "score_1h", pTeam->Get_Score_First_Half() );
			CreateJSOEntry_Number( pNewTeamObject, "score_2h", pTeam->Get_Score_Second_Half() );
		}

		if ( CSGameRules()->GetOvertimePlaying() )
		{
			CreateJSOEntry_Number( pNewTeamObject, "score_ot", pTeam->Get_Score_Overtime() );
		}

		if ( pTeam->m_bSurrendered )
		{
			CreateJSOEntry_Number( pNewTeamObject, "surrendered", pTeam->m_bSurrendered );
		}
			
		CreateJSOEntry_Number( pNewTeamObject, "map_victories", pTeam->m_numMapVictories );

	}

	int nMaxRounds = MAX_MATCH_STATS_ROUNDS;// CUiComponent_MatchStats::DoesSupportOvertimeStats() ? CSGameRules()->GetTotalRoundsPlayedThisPeriod() : CSGameRules()->GetTotalRoundsPlayed();

	nMaxRounds = Min( nMaxRounds, MAX_MATCH_STATS_ROUNDS);

	nMaxRounds -= 1; // turn 1-based round to 0-based index

	v8::Local< v8::Object > pRoundsObject = CreateJSOSubObject( pObject, "rounddata" );
	for ( int r = 0; r <= nMaxRounds; r++ )
	{
		int iReason = CSGameRules()->m_iMatchStats_RoundResults.Get( r );
		const char* result = "";

// 		for ( int i = 0; i < ARRAYSIZE( s_WinReasonNameLookup ); i++ )
// 		{
// 			if ( ( iReason == s_WinReasonNameLookup[ i ].iRoundResult ) )
// 			{
// 				result = s_WinReasonNameLookup[ i ].szResult;
// 				break;
// 			}
// 		}

		v8::Local< v8::Object > pNewRoundDataObject = CreateJSOSubObject( pRoundsObject, CFmtStr( "%d", r + 1 ).Get() );

		CreateJSOEntry_Number( pNewRoundDataObject, "round", r + 1 );
		CreateJSOEntry_String( pNewRoundDataObject, "result", result );

		CreateJSOEntry_Number( pNewRoundDataObject, "players_alive_CT", CSGameRules()->m_iMatchStats_PlayersAlive_CT.Get( r ) );
		CreateJSOEntry_Number( pNewRoundDataObject, "players_alive_TERRORIST", CSGameRules()->m_iMatchStats_PlayersAlive_T.Get( r ) );
	}

	args.GetReturnValue().Set( pObject );
}

#endif	// PANORAMA_ENABLE


extern ConVar mp_maxrounds;
extern ConVar mp_overtime_maxrounds;

int CUiComponent_GameState::GetRoundsRemaining()
{
	int numRoundsToEndMatch = mp_maxrounds.GetInt() + ( CSGameRules() ? CSGameRules()->GetOvertimePlaying() * mp_overtime_maxrounds.GetInt() : 0 );

	return ( numRoundsToEndMatch - CSGameRules()->GetTotalRoundsPlayed() );
}

char const * CUiComponent_GameState::GetServerName()
{
	static char szReturn[ MAX_PLAYER_NAME_LENGTH ] = "";
	szReturn[ 0 ] = 0;

	char const *szTournamentEventName = CSGameRules() ? CSGameRules()->GetTournamentEventName() : NULL;
	if ( szTournamentEventName && *szTournamentEventName )
	{

		if ( const wchar_t *wsz = g_pVGuiLocalize->Find( szTournamentEventName ) )
			V_WStringToUTF8( wsz, szReturn, sizeof( szReturn ) );

	}
// 	else if ( m_szHostName[ 0 ] && !CUiComponent_MatchStats::IsServerWhitelistedValveOfficial() )
// 	{
// 		V_WStringToUTF8( m_szHostName, szReturn, sizeof( szReturn ) );
// 	}

	return szReturn;
}

char const * CUiComponent_GameState::GetMapName()
{
	static char szReturn[ 128 ];

	V_WStringToUTF8( CCSGameRules::GetFriendlyMapName( engine->GetLevelNameShort() ), szReturn, sizeof( szReturn ) );
	return szReturn;

}

char const * CUiComponent_GameState::GetTournamentEventStage()
{
	static char szReturn[ MAX_PLAYER_NAME_LENGTH ] = "";
	szReturn[ 0 ] = 0;

	char const *szTournamentEventStage = CSGameRules() ? CSGameRules()->GetTournamentEventStage() : NULL;
	if ( szTournamentEventStage && *szTournamentEventStage )
	{
		if ( const wchar_t *wsz = g_pVGuiLocalize->Find( szTournamentEventStage ) )
			V_WStringToUTF8( wsz, szReturn, sizeof( szReturn ) );
	}

	return szReturn;
}

char const * CUiComponent_GameState::GetMapBSPName()
{
	// returns "unconnected" if we're not yet in a level.
	return engine->GetLevelNameShort();
}

char const * CUiComponent_GameState::GetGameModeInternalName( bool bUseSkirmishName )
{
	const char* gameModeName = g_pGameTypes->GetCurrentModeName();

	static ConVarRef sv_skirmish_id( "sv_skirmish_id" );
	if ( bUseSkirmishName && sv_skirmish_id.GetInt() != 0 )
	{
		if ( const CSkirmishModeDefinition* pSkirmishDef = GetItemSchema()->GetSkirmishModeDefinition( sv_skirmish_id.GetInt() ) )
		{
			// If in skirmish, replace (for example) "Casual" with "Flying Scoutsman"
			gameModeName = pSkirmishDef->GetName();
		}
	}

	return gameModeName ? gameModeName : "";
}

char const * CUiComponent_GameState::GetGameModeImagePath()
{
	const char* gameModeIconName = g_pGameTypes->GetCurrentModeName();

	static ConVarRef sv_skirmish_id( "sv_skirmish_id" );
	if ( sv_skirmish_id.GetInt() != 0 )
	{
		if ( const CSkirmishModeDefinition* pSkirmishDef = GetItemSchema()->GetSkirmishModeDefinition( sv_skirmish_id.GetInt() ) )
		{
			gameModeIconName = pSkirmishDef->GetIcon();
		}
	}

	static CFmtStr result;

	return result.Format( "file://{images}/icons/ui/%s.svg", gameModeIconName );
}



char const * CUiComponent_GameState::GetGameModeName( bool bUseSkirmishName )
{
	const char* gameModeNameID = g_pGameTypes->GetCurrentGameModeNameID();

	static ConVarRef sv_skirmish_id( "sv_skirmish_id" );
	if ( bUseSkirmishName && sv_skirmish_id.GetInt() != 0 )
	{
		if ( const CSkirmishModeDefinition* pSkirmishDef = GetItemSchema()->GetSkirmishModeDefinition( sv_skirmish_id.GetInt() ) )
		{
			// If in skirmish, replace (for example) "Casual" with "Flying Scoutsman"
			gameModeNameID = pSkirmishDef->GetLocNameToken();
		}
	}

	if ( gameModeNameID != nullptr )
	{
		wchar_t* gameModeString = g_pVGuiLocalize->Find( gameModeNameID );
		if ( gameModeString )
		{
			static char szReturn[128] = { 0 };
			V_WStringToUTF8( gameModeString, szReturn, sizeof( szReturn ) );
			return szReturn;
		}
	}

	return "";
}


int CUiComponent_GameState::GetViewerCount()
{
	return 0;
}


bool CUiComponent_GameState::HasHalfTime()
{
	return CSGameRules()->HasHalfTime();
}


bool CUiComponent_GameState::IsQueuedMatchmaking()
{
	return CSGameRules() && CSGameRules()->IsQueuedMatchmaking();
}

bool CUiComponent_GameState::IsEndMatchMapVoteEnabled()
{
	return CSGameRules() && CSGameRules()->IsEndMatchVotingForNextMapEnabled();
}

void CUiComponent_GameState::PostInit()
{
	//ListenForGameEvent( "announce_phase_end" );
	ListenForGameEvent( "server_spawn" );
	ListenForGameEvent( "begin_new_match" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "cs_match_end_restart" );
	ListenForGameEvent( "round_announce_match_start" );
	ListenForGameEvent( "round_mvp" );
	
	/* 
	
	// event-driven updates are ideal but have unresolved issues, such as events happening before the playerresource is updated. 
	// Current implementation updates at 10hz.

 	ListenForGameEvent( "bomb_planted" );
 	ListenForGameEvent( "bomb_defused" );
 	ListenForGameEvent( "player_spawn" );
 	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_score" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "bomb_dropped" );
	ListenForGameEvent( "bomb_pickup" );

	
	ListenForGameEvent( "round_end" ); 

	ListenForGameEvent( "cs_match_end_restart" );
	
	*/


}

void CUiComponent_GameState::FireGameEvent( IGameEvent *event )
{
#if defined ( PANORAMA_ENABLE )

	const char *type = event->GetName();

//	int nPlayerIndex;

	/*
	if ( !V_strcmp( type, "announce_phase_end" ) )
	{
		Assert( CSGameRules() );
		if ( CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME
			|| CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED )
		{
			// Freeze current game state
			Latch();
		}
		else
		{
			// use updating game state
			Unlatch();
		}
	}
	*/

	if ( !V_strcmp( type, "server_spawn" ) )
	{
		Unlatch();

		const char *hostname = event->GetString( "hostname" );
		
		wchar_t wszName[ 128 ];
		g_pVGuiLocalize->ConvertANSIToUnicode( hostname, wszName, sizeof( wszName ) );
		g_pVGuiLocalize->ConstructString( m_szHostName, sizeof( m_szHostName ), g_pVGuiLocalize->Find( "#SFUI_Scoreboard_ServerName" ), 1, wszName );
		// The truncate player name function is just a generic truncate.  Use it to truncate the server name.
		TruncatePlayerName( m_szHostName, ARRAYSIZE( m_szHostName ), 80, true );

		m_iFreeForAllModeWinnerIndex = -1;
		m_bServerRankRevealAll = false;
		m_flTimeWhenShudownHappens = 0;
		m_flTimeWhenClientShouldDropFromGotv = 0;
		panorama::DispatchEvent( GameState_ServerSpawn(), NULL );
	}
	else if ( ( !V_strcmp( type, "begin_new_match" ) ) ||
		( !V_strcmp( type, "cs_match_end_restart" ) ) ||
		( !V_strcmp( type, "round_announce_match_start" ) ) )
	{
		m_bServerRankRevealAll = false;
		panorama::DispatchEvent( GameState_OnMatchStart(), NULL );

		// when all player stats would get reset, we want to do it without the visual 'pulse' so silent=true
		panorama::DispatchEvent( GameState_UpdateAllPlayers(), NULL, true /* bSilent */ );
	}
	else if ( !V_strcmp( type, "round_mvp" ) && CSGameRules() && ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameDeathmatch() ) )
	{
		int nUserId = event->GetInt( "userid" );
		if ( C_BasePlayer* pPlayer = UTIL_PlayerByUserId( nUserId ) )
			m_iFreeForAllModeWinnerIndex = pPlayer->entindex();
		else
			m_iFreeForAllModeWinnerIndex = -1;
	}
	else if ( !V_strcmp( type, "round_start" ) )
	{
		Unlatch(); // halftime might have ended

		m_iFreeForAllModeWinnerIndex = -1;
		m_flTimeWhenClientShouldDropFromGotv = 0;

		// while max_rounds is more than 1, we need this to reset scoreboard for testing.
		if ( CSGameRules()->IsPlayingSurvival() )
		{
			panorama::DispatchEvent( GameState_OnMatchStart(), NULL );
		}
	}

	


#endif	// PANORAMA_ENABLE
}


void CUiComponent_GameState::Update( float frametime )
{
	if ( !CSGameRules() )
		return;

	if ( !GetCSResources() )
		return;


	FOR_EACH_VEC( m_vecGameTimeLabels, i )
	{
		Assert( m_vecGameTimeLabels[ i ].Get() );

		if ( !m_vecGameTimeLabels[i].Get() || !m_vecGameTimeLabels[ i ]->IsActive() )
			continue;

		int nTime = -1;
		const char * szFormat = "";

		if ( CSGameRules()->IsWarmupPeriod() )
		{
			szFormat = "#Time_Warmup";
	
			if ( !CSGameRules()->IsWarmupPeriodPaused() )
			{
				nTime = static_cast<int>( ceil( CSGameRules()->GetWarmupPeriodEndTime() - gpGlobals->curtime ) );
				
			}
		}
		else switch ( m_vecGameTimeLabels[ i ]->m_eType )
		{
			case CCSGO_GameTimeLabel::TIMER_ROUND_REMAINING:
				if ( CSGameRules()->IsFreezePeriod() )
				{
					// countdown to the start of the round while we're in freeze period
					nTime = static_cast< int >( ceil( CSGameRules()->GetRoundStartTime() - gpGlobals->curtime ) );
					szFormat = "#Time_Freezetime";
				}
				else
				{
					if ( !CSGameRules()->m_iRoundWinStatus )
					{
						if ( !CSGameRules()->m_bBombPlanted )
						{
							nTime = static_cast< int >( ceil( CSGameRules()->GetRoundRemainingTime() ) );
							szFormat = "#Time_Remaining";
						}
						else
						{
							nTime = -1;
							szFormat = "#Time_Bomb_Planted";
						}
					}
				}
				break;

			case CCSGO_GameTimeLabel::TIMER_ROUND_ELAPSED:
				nTime = Max( 0, static_cast<int>( ceil( CSGameRules()->GetRoundElapsedTime() ) ) );
				break;

			case CCSGO_GameTimeLabel::TIMER_MAP_REMAINING:
				nTime = Max( 0, static_cast<int>( ceil( CSGameRules()->GetMapRemainingTime() ) ) );
				szFormat = "#Time_Remaining";
				break;


			case CCSGO_GameTimeLabel::TIMER_MAP_ELAPSED:
				nTime = Max( 0, static_cast<int>( ceil( CSGameRules()->GetMapElapsedTime() ) ) );
				szFormat = "#Time_Elapsed";
				break;

			default:
				//int numRoundsToEndMatch = mp_maxrounds.GetInt() + ( CSGameRules()->GetOvertimePlaying() * mp_overtime_maxrounds.GetInt() );


				const char * szNextMap = "";

				if ( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED )
				{
					if ( GetCSResources()->EndMatchNextMapAllVoted() )
					{
						if ( nextlevel.GetString()[ 0 ] )
						{
							nTime = ( int )CSGameRules()->GetTimeUntilNextPhaseStarts();
							szFormat = "#Time_MatchStartIn";  //"#Time_NextMapIn";
							szNextMap = nextlevel.GetString();
						}
						else
						{
							nTime = ( int )CSGameRules()->GetTimeUntilNextPhaseStarts();
							szFormat = "#Time_NextMatchIn";
						}
					}
					else if ( CSGameRules()->IsEndMatchVotingForNextMap() )
					{
						nTime = static_cast<int>( ceil( CSGameRules()->GetTimeUntilNextPhaseStarts() ) );
						szFormat = "#Time_MapVoteEndIn";
					}
					else if ( CSGameRules()->IsQueuedMatchmaking() && m_flTimeWhenShudownHappens && ( Plat_FloatTime() < m_flTimeWhenShudownHappens ) )
					{
						nTime = static_cast<int>( ceil( m_flTimeWhenShudownHappens - Plat_FloatTime() ) );
						szFormat = "#Time__MapShutdownIn";

						// If it's time for this client to disconnect then do so
						// THIS WAS PORTED FROM SCALEFORM SCOREBOARD BUT DISCONNECT DOES NOT BELONG IN SCOREBOARD. MOVE IT.
						if ( m_flTimeWhenClientShouldDropFromGotv && ( Plat_FloatTime() > m_flTimeWhenClientShouldDropFromGotv ) )
						{
							engine->ClientCmd_Unrestricted( "disconnect" );
						}

					}
					else if ( !CSGameRules()->IsQueuedMatchmaking() )
					{
						if ( nextlevel.GetString()[ 0 ] )
						{

							nTime = static_cast<int>( ceil( CSGameRules()->GetTimeUntilNextPhaseStarts() ) );
							szFormat = "#Time_MatchStartIn";  //"#Time_NextMapIn";
							szNextMap = nextlevel.GetString();
						}
						else
						{
							nTime = static_cast<int>( ceil( CSGameRules()->GetTimeUntilNextPhaseStarts() ) );
							szFormat = "#Time_MatchStartIn";
						}
					}
					else
					{
						nTime = 0;
						szFormat = "#Time_MatchStartIn"; //"#Time_NextMapIn";
					}
				}
				else if ( CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME ) //If we are entering halftime, show the time till team switch
				{
					nTime = static_cast<int>( ceil( CSGameRules()->GetTimeUntilNextPhaseStarts() ) );

					if ( CSGameRules()->GetOvertimePlaying() &&
						( !mp_overtime_maxrounds.GetInt() || !( ( CSGameRules()->GetTotalRoundsPlayed() - mp_maxrounds.GetInt() ) % mp_overtime_maxrounds.GetInt() ) ) )
					{
						szFormat = "#Time_OvertimeIn";
					}
					else
					{
						szFormat = "#Time_TeamSwitchIn";
					}
				}
				else
				{
					if ( CSGameRules()->IsFreezePeriod() )
					{
						// countdown to the start of the round while we're in freeze period
						nTime = static_cast< int >( ceil( CSGameRules()->GetRoundStartTime() - gpGlobals->curtime ) );
						szFormat = "#Time_Freezetime";
					}
					else
					{
						if ( !CSGameRules()->m_iRoundWinStatus )
						{
							if ( !CSGameRules()->m_bBombPlanted )
							{
								nTime = static_cast< int >( ceil( CSGameRules()->GetRoundRemainingTime() ) );
								szFormat = "#Time_Remaining";
							}
							else
							{
								nTime = -1;
								szFormat = "#Time_Bomb_Planted";
							}
						}
					}
					break;
				}


				if ( m_vecGameTimeLabels[ i ]->m_szNextMap != szNextMap )
				{
					m_vecGameTimeLabels[ i ]->m_szNextMap = szNextMap;
					m_vecGameTimeLabels[ i ]->SetDialogVariable( "s_gametime_nextmap", szNextMap );
				}

				break;
		}


		if ( m_vecGameTimeLabels[ i ]->m_nTime != nTime )
		{
			m_vecGameTimeLabels[ i ]->m_nTime = nTime;

			char szTime[ 32 ];
			szTime[ 0 ] = 0;

			if ( nTime >= 0 )
			{
				int nMinutes = nTime / 60;
				int nSeconds = nTime % 60;

				V_snprintf( szTime, ARRAYSIZE( szTime ), "%d:%.2d", nMinutes, nSeconds );
			}

			m_vecGameTimeLabels[ i ]->SetDialogVariable( "s_gametime_time", szTime );
		}

		if ( m_vecGameTimeLabels[ i ]->m_szTimeFormat != szFormat )
		{
			m_vecGameTimeLabels[ i ]->m_szTimeFormat = szFormat;

			panorama::CLocStringSafePointer pchFormat = panorama::UILocalize()->PchFindToken( nullptr, szFormat,
				panorama::k_nLocalizeMaxChars,
				panorama::k_eStringTruncationStyle_None,
				panorama::k_eStringTransformStyle_None,
				panorama::k_eStringEscapeStyle_None );

			panorama::CLocStringSafePointer pchFormatAgain = panorama::UILocalize()->PchFindToken( nullptr, pchFormat->String(),
				panorama::k_nLocalizeMaxChars,
				panorama::k_eStringTruncationStyle_None,
				panorama::k_eStringTransformStyle_None,
				panorama::k_eStringEscapeStyle_None );

			m_vecGameTimeLabels[ i ]->SetDialogVariable( "s_gametime_desc", pchFormatAgain->String() );
		}
	}
}



void CUiComponent_GameState::LevelInitPostEntity()
{
	panorama::DispatchEvent( GameState_OnLevelLoad(), NULL );

	m_szLocalId = "";

	// shouldn't be latched during regular gameplay
	Unlatch();
}

void CUiComponent_GameState::LevelShutdownPreEntity()
{
	// shouldn't be latched when not in a level
	Unlatch();
}

void CUiComponent_GameState::AllRanksReveal( int numSecondsTillShutdown )
{
	if ( numSecondsTillShutdown )
	{
		m_flTimeWhenShudownHappens = Plat_FloatTime() + numSecondsTillShutdown;

		if ( g_bEngineIsHLTV && ( numSecondsTillShutdown > 2 ) )
			m_flTimeWhenClientShouldDropFromGotv = Plat_FloatTime() + 1 + RandomFloat()*( numSecondsTillShutdown - 2 );
	}

	m_bServerRankRevealAll = true;

	// tell the panorama scoreboard to update everyone's skill group
	panorama::DispatchEvent( GameState_RankRevealAll(), NULL );
}



void CUiComponent_GameState::Latch()
{
	if ( IsLatched() )
		return;	// Maybe we should merge _some_ new information in this case?

	C_CS_PlayerResource* rsrc = GetCSResources();
	if ( !rsrc )
		return;

	// copy C_Latchable* state from resources
	m_latchedGameState = *rsrc;
	m_latchedGameStateCS = *rsrc;

	// save off any extra state from the game we care about
	m_latchedExtraState.Latch(); // sets IsLatched to true
}

void CUiComponent_GameState::Unlatch()
{
	if ( !m_latchedExtraState.IsLatched() )
		return;

	m_latchedExtraState.Unlatch();
}

CUiComponent_GameState::CExtraLatchedState::CExtraLatchedState()
{
	// reset data by faking an 'unlatch' on initialization
	// so we don't need to duplicate that logic
	m_bLatched = true;
	Unlatch();
}

void CUiComponent_GameState::CExtraLatchedState::Unlatch()
{
	m_bLatched = false;

	// free extra stuff allocated during latching
	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		m_bIsFakePlayer[i] = false;
		m_nAccount[i] = -1;
		m_DecoratedPlayerNames[i].Clear();
		m_DecoratedHTMLPlayerNames[i].Clear();
	}
}

void CUiComponent_GameState::CExtraLatchedState::Latch()
{
	if ( m_bLatched )
		return;

	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		m_bIsFakePlayer[i] = IsFakePlayer( i );
		m_DecoratedPlayerNames[i] = GetDecoratedPlayerName( i, false );
		m_DecoratedHTMLPlayerNames[i] = GetDecoratedPlayerName( i, true );
		m_nAccount[i] = GetPlayerMoney( i );
		m_xuids[i] = GetXuid( i );
	}

	// we are now latched
	m_bLatched = true;
}

bool CUiComponent_GameState::CExtraLatchedState::IsFakePlayer( int idxPlayer )
{
	if ( idxPlayer < 0 || idxPlayer >= MAX_PLAYERS )
		return false;

	if ( m_bLatched )
		return m_bIsFakePlayer[idxPlayer];

	if ( !GetCSResources() )
		return false;

	return GetCSResources()->IsFakePlayer( idxPlayer );
}

bool CUiComponent_GameState::CExtraLatchedState::IsHLTV( int idxPlayer )
{
	if ( !GetCSResources() )
		return false;

	// Don't need to latch this, hltv players never connect/disconnect.
	return GetCSResources()->IsHLTV( idxPlayer );
}

int CUiComponent_GameState::CExtraLatchedState::GetPlayerMoney( int nPlayerIndex )
{
	if ( nPlayerIndex < 0 || nPlayerIndex >= MAX_PLAYERS )
		return -1;

	if ( m_bLatched )
		return m_nAccount[nPlayerIndex];

	if ( !GetCSResources() )
		return -2;

	C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( nPlayerIndex ) );
	if ( !pPlayer )
		return -3;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return -4;

	if ( pLocalPlayer->GetAssociatedTeamNumber() == GetCSResources()->GetTeam( nPlayerIndex ) ||
		pLocalPlayer->IsSpectator() ||
		CUiComponent_GameState::GetInstance()->IsDemoOrHltv() )

	{
		return pPlayer->GetAccount(); // Might be wrong if player is out of PVS?  I guess teammates are never out of PVS?
	}

	return -5;
}

uint64 CUiComponent_GameState::CExtraLatchedState::GetXuid( int nPlayerIndex )
{
	if ( nPlayerIndex < 0 || nPlayerIndex >= MAX_PLAYERS )
		return 0;

	if ( m_bLatched )
		return m_xuids[nPlayerIndex];

	if ( nPlayerIndex == 0 )
		return 0;

	if ( IsHLTV( nPlayerIndex ) )
		return 0;

#if defined( OSX ) && defined( __aarch64__ ) && defined( NO_STEAM )
	return nPlayerIndex;
#endif
	// bots use player index instead of XUIDs.
	if ( IsFakePlayer( nPlayerIndex ) )
		return nPlayerIndex;

	// we don't have XUIDs in overwatch demos.
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
		{
			return nPlayerIndex;
		}
	}

	CSteamID steamID;
	C_BasePlayer::GetSteamID( nPlayerIndex, &steamID );

	return steamID.ConvertToUint64();
}

