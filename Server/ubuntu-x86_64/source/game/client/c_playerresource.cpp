//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "gamestringpool.h"
#include "hltvreplaysystem.h"
#include "clientsteamcontext.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

#ifdef PANORAMA_ENABLE
#include "panorama/csgo_avatarimage.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const float PLAYER_RESOURCE_THINK_INTERVAL = 0.2f;
#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"

ConVar cl_names_debug( "cl_names_debug", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_sanitize_player_names( "cl_sanitize_player_names", "0", FCVAR_ARCHIVE, "Replace names of other players with something non-offensive." );

void RecvProxy_ChangedTeam( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Have the regular proxy store the data.
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	if ( g_PR )
	{
		g_PR->TeamChanged();
	}
}

IMPLEMENT_CLIENTCLASS_DT_NOBASE(C_PlayerResource, DT_PlayerResource, CPlayerResource)
	RecvPropArray3( RECVINFO_ARRAY(m_iPing), RecvPropInt( RECVINFO(m_iPing[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iKills), RecvPropInt( RECVINFO(m_iKills[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iAssists), RecvPropInt( RECVINFO(m_iAssists[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iDeaths), RecvPropInt( RECVINFO(m_iDeaths[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_bConnected), RecvPropInt( RECVINFO(m_bConnected[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iTeam), RecvPropInt( RECVINFO(m_iTeam[0]), 0, RecvProxy_ChangedTeam )),
	RecvPropArray3( RECVINFO_ARRAY(m_iPendingTeam), RecvPropInt( RECVINFO(m_iPendingTeam[0]), 0, RecvProxy_ChangedTeam )),
	RecvPropArray3( RECVINFO_ARRAY(m_bAlive), RecvPropInt( RECVINFO(m_bAlive[0]))),
	RecvPropArray3( RECVINFO_ARRAY_NAME(C_PlayerResourceLatchableGameState::m_iHealth, m_iHealth), RecvPropInt( RECVINFO_NAME(C_PlayerResourceLatchableGameState::m_iHealth[0], m_iHealth[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iCoachingTeam), RecvPropInt( RECVINFO(m_iCoachingTeam[0]))),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_PlayerResource )

	DEFINE_PRED_ARRAY( m_szName, FIELD_STRING, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iPing, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iKills, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iAssists, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iDeaths, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_bConnected, FIELD_BOOLEAN, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iTeam, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iPendingTeam, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_bAlive, FIELD_BOOLEAN, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY_AMBIGUOUS( C_PlayerResourceLatchableGameState, m_iHealth, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iCoachingTeam, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),

END_PREDICTION_DATA()	

C_PlayerResource *g_PR;

IGameResources * GameResources( void ) { return g_PR; }

C_PlayerResourceLatchableGameState::C_PlayerResourceLatchableGameState()
{
	for ( int i = 0; i < ARRAYSIZE( m_szName ); ++i )
	{
		m_szName[i] = nullptr;
	}
	memset( m_iPing, 0, sizeof( m_iPing ) );
	//	memset( m_iPacketloss, 0, sizeof( m_iPacketloss ) );
	memset( m_iKills, 0, sizeof( m_iKills ) );
	memset( m_iAssists, 0, sizeof( m_iAssists ) );
	memset( m_iDeaths, 0, sizeof( m_iDeaths ) );
	memset( m_bConnected, 0, sizeof( m_bConnected ) );
	memset( m_iTeam, 0, sizeof( m_iTeam ) );
	memset( m_iPendingTeam, 0, sizeof( m_iTeam ) );
	memset( m_bAlive, 0, sizeof( m_bAlive ) );
	memset( m_iHealth, 0, sizeof( m_iHealth ) );
	memset( m_Xuids, 0, sizeof( m_Xuids ) );
	memset( m_iCoachingTeam, 0, sizeof( m_iCoachingTeam ) );

	for ( int i = 0; i < MAX_TEAMS; i++ )
	{
		m_Colors[i] = COLOR_GREY;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PlayerResource::C_PlayerResource()
{
	for ( int i=0; i<ARRAYSIZE(m_szName); ++i )
	{
		m_szName[i] = AllocPooledString( "unconnected" );
	}

	g_PR = this;

#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI->AddDeviceDependentObject( this );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PlayerResource::~C_PlayerResource()
{
	// TODONOSF

	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		if ( m_Xuids[i] != INVALID_XUID )
		{
#if defined( INCLUDE_SCALEFORM )
			g_pScaleformUI->AvatarImageRelease( m_Xuids[ i ] );
#endif
		}
	}

	g_PR = NULL;

#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI->RemoveDeviceDependentObject( this );
#endif
}

void C_PlayerResource::OnDataChanged(DataUpdateType_t updateType)
{
	UpdateXuids();

	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
	}
}

void C_PlayerResource::UpdateXuids( void )
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		XUID newXuid = INVALID_XUID;
		player_info_t sPlayerInfo;
		if ( m_bConnected[ i ] && engine->GetPlayerInfo( i, &sPlayerInfo ) )
		{
			newXuid = sPlayerInfo.xuid;

			// When running in anonymous mode wipe out all XUIDs
			if ( newXuid != INVALID_XUID )
			{
				if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
				{
					if ( pParameters->m_bAnonymousPlayerIdentity )
					{
// 						CSteamID steamid( newXuid );
// 						steamid.SetAccountID( ( ~uint32(0) ) -1 - i );
// 						newXuid = steamid.ConvertToUint64();
						newXuid = INVALID_XUID;
					}
				}
			}
		}

		if ( newXuid != m_Xuids[i] )
		{
			// TODONOSF

#if defined( INCLUDE_SCALEFORM )
			bool bAddRefSuccess = false;

			if ( m_Xuids[i] != INVALID_XUID )
			{
				g_pScaleformUI->AvatarImageRelease( m_Xuids[ i ] );
			}

			if ( newXuid != INVALID_XUID )
			{
				bAddRefSuccess = g_pScaleformUI->AvatarImageAddRef( newXuid );
			}

			if ( bAddRefSuccess || ( newXuid == INVALID_XUID ) )
			{
				m_Xuids[i] = newXuid;
			}
#else
#ifdef PANORAMA_ENABLE
			if ( newXuid != INVALID_XUID )
			{
				// Preloading medium/small avatar images
				CSteamID steamID( newXuid );
				g_AvatarImageMgr.GetMediumSteamAvatar( steamID );
				g_AvatarImageMgr.GetSmallSteamAvatar( steamID );
			}
#endif // PANORAMA_ENABLE
			m_Xuids[i] = newXuid;
#endif
		}
	}
}

const char* C_PlayerResource::SetupLocalizedFakePlayerName( int idxPlayer, char const *pchPlayerName, char (&scratch)[PLAYER_RESOURCE_SCRATCH_LENGTH] )
{
	COMPILE_TIME_ASSERT( sizeof( scratch ) > MAX_PLAYER_NAME_LENGTH );
	static CUtlDict< CUtlConstString > s_mapLocalizedNames;
	int mapIdx = s_mapLocalizedNames.Find( pchPlayerName );

	if ( mapIdx == s_mapLocalizedNames.InvalidIndex() && strlen( pchPlayerName ) < 32 ) // none of our localized bot names are that long
	{
		// Check if we can localize this name
		if ( const wchar_t* kwszLocalized = g_pVGuiLocalize->Find( CFmtStr( "#CSGO_FakePlayer_%s", pchPlayerName ) ) )
		{
			// Save localized name so we don't have to re-do this work later
			V_UnicodeToUTF8( kwszLocalized, scratch, sizeof( scratch ) );
			mapIdx = s_mapLocalizedNames.Insert( pchPlayerName, scratch );
		}
	}

	if ( mapIdx != s_mapLocalizedNames.InvalidIndex() )
	{
		return s_mapLocalizedNames.Element( mapIdx );
	}

	return pchPlayerName;
}

void C_PlayerResource::ClientThink()
{
	BaseClass::ClientThink();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		UpdatePlayerName( i );
	}

	SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
}

#define STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_chStaticArrayVarName, szLocToken, szDefaultText ) \
	static char s_chStaticArrayVarName[MAX_PLAYER_NAME_LENGTH] = {}; \
	if ( !s_chStaticArrayVarName[ 0 ] ) \
	{ \
		wchar_t const * const kwszTheSuspect = g_pVGuiLocalize->Find( szLocToken ); \
		Assert( kwszTheSuspect ); \
		V_UnicodeToUTF8( kwszTheSuspect, s_chStaticArrayVarName, sizeof( s_chStaticArrayVarName ) ); \
		Assert( s_chStaticArrayVarName[ 0 ] ); \
		if ( !s_chStaticArrayVarName[ 0 ] ) \
			V_strcpy_safe( s_chStaticArrayVarName, szDefaultText ); \
	}


class CStaticPlayerNamesSet
{
public:
	CStaticPlayerNamesSet()
	{
		m_bInitialized = false;
		Q_memset( m_szNames, 0, sizeof( m_szNames ) );
	}

	char const * GetName( int idx )
	{
		if ( ( idx < 0 ) || ( idx > MAX_PLAYERS ) )
		{
			STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8LocalPlayer, "#SFUI_LocalPlayer", "Player" );
			return s_utf8LocalPlayer;
		}
		else
		{
			if ( !m_bInitialized )
				InitializeNames();

			return m_szNames[idx];
		}
	}

protected:
	bool m_bInitialized;
	char const * m_szNames[MAX_PLAYERS+1];

public:
	void InitializeNames()
	{
		CUtlVector< const char * > arrNamesOptions;
#define RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( englishName ) { \
	STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8Name##englishName, "#CSGO_FakePlayer_" #englishName, #englishName ); \
	arrNamesOptions.AddToTail( s_utf8Name##englishName ); }
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Albatross );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Alpha );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Anchor );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Banjo );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Bell );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Beta );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Blackbird );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Bulldog );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Canary );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Cat );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Calf );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Cyclone );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Daisy );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Dalmatian );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Dart );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Delta );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Diamond );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Donkey );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Duck );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Emu );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Eclipse );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Flamingo );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Flute );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Frog );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Goose );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Hatchet );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Heron );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Husky );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Hurricane );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Iceberg );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Iguana );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Kiwi );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Kite );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Lamb );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Lily );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Macaw );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Manatee );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Maple );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Mask );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Nautilus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Ostrich );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Octopus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Pelican );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Puffin );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Pyramid );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Rattle );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Robin );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Rose );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Salmon );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Seal );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Shark );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sheep );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Snake );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sonar );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Stump );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sparrow );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Toaster );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Toucan );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Torus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Violet );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Vortex );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Vulture );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Wagon );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Whale );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Woodpecker );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Zebra );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Zigzag );
#undef RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED
		for ( int k = 0; k < MAX_PLAYERS + 1; ++ k )
		{
			if ( !arrNamesOptions.Count() )
				Error( "Insufficient random names pool!\n" );
			int iRandomChoice = RandomInt( 0, arrNamesOptions.Count() - 1 );
			m_szNames[k] = arrNamesOptions.Element( iRandomChoice );
			arrNamesOptions.Remove( iRandomChoice );
		}
		m_bInitialized = true;
	}
} g_staticPlayerNames;
void EnsureStaticPlayerNamesReinitialized()
{
	g_staticPlayerNames.InitializeNames();
}

//-----------------------------------------------------------------------------
// Purpose: Update a player's name
//-----------------------------------------------------------------------------
void C_PlayerResource::UpdatePlayerName( int idxPlayer )
{
	if ( idxPlayer < 1 || idxPlayer > MAX_PLAYERS )
	{
		Error( "UpdatePlayerName with bogus slot %d\n", idxPlayer );
		return;
	}

	char scratchBuf[PLAYER_RESOURCE_SCRATCH_LENGTH];
	const char* szPlayerName = SetupPlayerName( idxPlayer, scratchBuf );

	if ( szPlayerName && ( !m_szName[idxPlayer] || Q_stricmp( m_szName[idxPlayer], szPlayerName ) ) )
	{
		m_szName[idxPlayer] = AllocPooledString( szPlayerName );
	}
}

char const * C_PlayerResource::SanitizePlayerName( int idxPlayer )
{
	return g_staticPlayerNames.GetName( idxPlayer );
}

bool Helper_ShouldSanitizePlayerName( CSteamID steamID )
{
	if ( !cl_sanitize_player_names.GetBool() )
		return false;

	CSteamID steamIDLocal = ClientSteamContext().GetLocalPlayerSteamID();
	if ( steamIDLocal.GetAccountID() == steamID.GetAccountID() )
		return false;

	steamIDLocal.SetAccountID( steamID.GetAccountID() );
	// If steam is unavailable, assume this player is not a friend and error on the side of hiding names
	// PERF: We need a game side cache for this IPC call
	bool bIsFriend = steamapicontext && steamapicontext->SteamFriends() && steamIDLocal.IsValid() &&
		steamapicontext->SteamFriends()->HasFriend( steamIDLocal, k_EFriendFlagImmediate );

	return !bIsFriend;
}

char const * Helper_GetFriendPersonaNameSanitized( CSteamID steamID )
{
	if ( Helper_ShouldSanitizePlayerName( steamID ) )
	{
		return g_staticPlayerNames.GetName( steamID.GetAccountID() % ( MAX_PLAYERS + 1 ) );
	}

	return steamapicontext->SteamFriends()->GetFriendPersonaName( steamID );
}


const char* C_PlayerResource::SetupPlayerName( int idxPlayer, char (&scratch)[PLAYER_RESOURCE_SCRATCH_LENGTH] )
{
	player_info_t sPlayerInfo;
	char const *pchPlayerName = PLAYER_UNCONNECTED_NAME;
	if ( IsConnected( idxPlayer ) &&
		engine->GetPlayerInfo( idxPlayer, &sPlayerInfo ) )
	{
		V_strcpy_safe( scratch, sPlayerInfo.name );
		pchPlayerName = scratch;

		CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters();
		if ( pParameters &&  pParameters->m_bAnonymousPlayerIdentity )
		{
			if ( pParameters->m_uiLockFirstPersonAccountID &&
				( CSteamID( sPlayerInfo.xuid ).GetAccountID() == pParameters->m_uiLockFirstPersonAccountID ) )
			{
				STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8TheSuspect, "#CSGO_Overwatch_TheSuspect", "The Suspect" );
				pchPlayerName = s_utf8TheSuspect;
			}
			else
			{
				pchPlayerName = g_staticPlayerNames.GetName( idxPlayer );
			}

			// player name has been anonymized, return it directly
			return pchPlayerName;
		}

		if ( Helper_ShouldSanitizePlayerName( CSteamID( GetXuid( idxPlayer ) ) ) && !IsFakePlayer( idxPlayer ) )
		{
			pchPlayerName = SanitizePlayerName( idxPlayer );
		}

		if ( sPlayerInfo.fakeplayer && *pchPlayerName )
		{
			char szFakePlayerScratch[PLAYER_RESOURCE_SCRATCH_LENGTH];
			return SetupLocalizedFakePlayerName( idxPlayer, pchPlayerName, szFakePlayerScratch );
		}
	}

	return pchPlayerName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_PlayerResourceLatchableGameState::GetPlayerName( int iIndex )
{
	if ( cl_names_debug.GetInt() )
		return PLAYER_DEBUG_NAME;

	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return PLAYER_ERROR_NAME;
	}
	
	if ( !IsConnected( iIndex ) )
		return PLAYER_UNCONNECTED_NAME;

	// This gets updated in ClientThink, so it could be up to 1 second out of date, oh well.
	return m_szName[iIndex];
}

const char *C_PlayerResource::GetPlayerName( int iIndex )
{
	if ( IsConnected( iIndex ) )
	{
		// $$$REI Try disabling this sometime
		// X360TBD: Network - figure out why the name isn't set
		if ( !m_szName[iIndex] || !Q_stricmp( m_szName[iIndex], PLAYER_UNCONNECTED_NAME ) )
		{
			// If you get a full "reset" uncompressed update from server, then you can have NULLNAME show up in the scoreboard
			UpdatePlayerName( iIndex );
		}
	}

	return C_PlayerResourceLatchableGameState::GetPlayerName( iIndex );
}

bool C_PlayerResourceLatchableGameState::IsAlive(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return false;

	return m_bAlive[iIndex];
}

int C_PlayerResourceLatchableGameState::GetTeam(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return 0;
	}
	else
	{
		return m_iTeam[iIndex];
	}
}

int C_PlayerResourceLatchableGameState::GetPendingTeam(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return 0;
	}
	else
	{
		return m_iPendingTeam[iIndex];
	}
}

const char * C_PlayerResource::GetTeamName(int idxTeam)
{
	C_Team *team = GetGlobalTeam( idxTeam );

	if ( !team )
		return "Unknown";

	return team->Get_Name();
}

int C_PlayerResourceLatchableGameState::GetCoachingTeam(int idxPlayer)
{
	if ( idxPlayer < 1 || idxPlayer > MAX_PLAYERS )
	{
		Assert( false );
		return 0;
	}
	else
	{
		return m_iCoachingTeam[idxPlayer];
	}
}


int C_PlayerResource::GetTeamScore(int idxTeam)
{
	C_Team *team = GetGlobalTeam( idxTeam );

	if ( !team )
		return 0;

	return team->Get_Score();
}

int C_PlayerResourceLatchableGameState::GetFrags(int idxPlayer )
{
	return 666;
}

bool C_PlayerResource::IsLocalPlayer(int idxPlayer)
{
	C_BasePlayer *pPlayer =	C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return false;

	// HLTV replay will not set m_bLocalPlayer flag, in a sense there's no selected local player, we're observing everyone
	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		return false;

	return ( idxPlayer == pPlayer->entindex() );
}


bool C_PlayerResource::IsHLTV(int idxPlayer)
{
	if ( !IsConnected( idxPlayer ) )
		return false;

	// HLTV replay will not set m_bLocalPlayer flag, in a sense there's no selected local player, we're observing everyone
	if ( g_HltvReplaySystem.GetHltvReplayDelay() && C_BasePlayer::GetLocalPlayer()->index == idxPlayer )
		return true;  // local player is always HLTV in HLTV replay mode, even though the hltv property isn't set because we are in the past and replaying everything as it was (including no hltv flag set)

	player_info_t sPlayerInfo;
	
	if ( engine->GetPlayerInfo( idxPlayer, &sPlayerInfo ) )
	{
		return sPlayerInfo.ishltv;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_PlayerResource::IsFakePlayer( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	// Yuck, make sure it's up to date
	player_info_t sPlayerInfo;
	if ( engine->GetPlayerInfo( iIndex, &sPlayerInfo ) )
	{
		return sPlayerInfo.fakeplayer;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResourceLatchableGameState::GetPing( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPing[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
/*-----------------------------------------------------------------------------
int	C_PlayerResourceLatchableGameState::GetPacketloss( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPacketloss[iIndex];
}*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResourceLatchableGameState::GetKills( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iKills[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResourceLatchableGameState::GetAssists( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iAssists[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResourceLatchableGameState::GetDeaths( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iDeaths[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResourceLatchableGameState::GetHealth( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return C_PlayerResourceLatchableGameState::m_iHealth[iIndex];
}

const Color &C_PlayerResourceLatchableGameState::GetTeamColor(int idxTeam )
{
	if ( idxTeam < 0 || idxTeam >= MAX_TEAMS )
	{
		Assert( false );
		static Color blah;
		return blah;
	}
	else
	{
		return m_Colors[idxTeam];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_PlayerResourceLatchableGameState::IsConnected( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return false;
	else
		return m_bConnected[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
XUID C_PlayerResourceLatchableGameState::GetXuid( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return INVALID_XUID;
	else
		return m_Xuids[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Fills the given string with the player xuid.
//-----------------------------------------------------------------------------
void C_PlayerResourceLatchableGameState::FillXuidText( int iIndex, char *buf, int bufSize )
{
	Assert( buf && bufSize );
	if ( buf && bufSize )
	{
		XUID xuid = GetXuid( iIndex );

		buf[0] = '\0';
		V_snprintf( buf, bufSize, "%llu", xuid );
	}
}

void C_PlayerResource::DeviceLost( void )
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		if ( m_Xuids[i] != INVALID_XUID )
		{
#if defined( INCLUDE_SCALEFORM )
			g_pScaleformUI->AvatarImageRelease( m_Xuids[ i ] );
#endif
			m_Xuids[i] = INVALID_XUID;
		}
	}
}

void C_PlayerResource::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
	UpdateXuids();
}
