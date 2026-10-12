//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: CS's custom C_PlayerResource
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_CS_PLAYERRESOURCE_H
#define C_CS_PLAYERRESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "cs_shareddefs.h"
#include "c_playerresource.h"
#include "cstrike15_item_inventory.h"
#include "cstrike15_gcconstants.h"

#define MAX_DECORATED_PLAYER_NAME_LENGTH ( ( MAX_NETWORKID_LENGTH * 10 ) + 20 )


enum EDecoratedPlayerNameFlag_t
{
	k_EDecoratedPlayerNameFlag_Simple = 0,
	k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot = ( 1 << 0 ),
	k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer = ( 1 << 1 ),
	k_EDecoratedPlayerNameFlag_DontShowClanName = ( 1 << 2 ),
#ifdef PANORAMA_ENABLE
	k_EDecoratedPlayerNameFlag_DontHTMLEscapeString = 0,
	k_EDecoratedPlayerNameFlag_HTMLEscapeString = ( 1 << 3 ),
#else
	k_EDecoratedPlayerNameFlag_HTMLEscapeString = 0,
	k_EDecoratedPlayerNameFlag_DontHTMLEscapeString = ( 1 << 3 ),
#endif
	k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName = ( 1 << 4 ),
};

DEFINE_ENUM_BITWISE_OPERATORS( EDecoratedPlayerNameFlag_t );


struct C_CS_PlayerResourceLatchableGameState
{
	C_CS_PlayerResourceLatchableGameState();

	bool			IsVIP( int iIndex );
	bool			HasC4( int iIndex );
	bool			IsHostageAlive( int iIndex );
	bool			IsHostageFollowingSomeone( int iIndex );
	int				GetHostageEntityID( int iIndex );
	const Vector	GetBombsiteAPosition();
	const Vector	GetBombsiteBPosition();
	const Vector	GetHostageRescuePosition( int index );

	int				GetNumMVPs( int iIndex );
	bool			HasDefuser( int iIndex );
	bool			HasHelmet( int iIndex );
	int				GetArmor( int iIndex );
	int				GetScore( int iIndex );
	int				GetCompetitiveRanking( int iIndex );
	int				GetCompetitiveWins( int iIndex );
	EPlayerRankTypeID_t GetCompetitiveRankType( int iIndex );
	int				GetCompTeammateColor( int iIndex );
	Color			GetCompPlayerColorByID( int iColorID );
	int				GetTotalCashSpent( int iIndex );
	int GetGunGameLevel( int iIndex );
	int				GetCashSpentThisRound( int iIndex );
	const char		*GetClanTag( int index );
	int				GetActiveCoinRank( int iIndex );
	int				GetMusicID( int iIndex );
	int			    GetLifetime( int index );

	int GetPersonaDataPublicLevel( int iIndex );
	int GetPersonaDataPublicCommendsLeader( int iIndex );
	int GetPersonaDataPublicCommendsTeacher( int iIndex );
	int GetPersonaDataPublicCommendsFriendly( int iIndex );
	int				GetBotDifficulty( int index );

#if CS_CONTROLLABLE_BOTS_ENABLED
	bool			IsControllingBot( int index );
	int				GetControlledPlayer( int index );
	int				GetControlledByPlayer( int index );
#endif

	const wchar_t*	GetDecoratedPlayerName( C_PlayerResourceLatchableGameState *state, bool isBot, int idxPlayer, wchar_t* buffer, int buffsize, EDecoratedPlayerNameFlag_t flags );

protected:
	bool IsConnected_Copy( int iIndex ); // unfortunately need to copy this data from C_PlayerResourceLatchableGameState
	bool	m_bConnectedCopy[MAX_PLAYERS + 1];

protected:
	int		m_iPlayerC4;	// entity index of C4 carrier or 0
	int		m_iPlayerVIP;	// entity index of VIP player or 0
	Vector	m_bombsiteCenterA;
	Vector	m_bombsiteCenterB;

	bool	m_bHostageAlive[MAX_HOSTAGES];
	bool	m_isHostageFollowingSomeone[MAX_HOSTAGES];
	int		m_iHostageEntityIDs[MAX_HOSTAGES];

	int		m_hostageRescueX[MAX_HOSTAGE_RESCUES];
	int		m_hostageRescueY[MAX_HOSTAGE_RESCUES];
	int		m_hostageRescueZ[MAX_HOSTAGE_RESCUES];

	int		m_iMVPs[MAX_PLAYERS + 1];
	bool	m_bHasDefuser[MAX_PLAYERS + 1];
	bool	m_bHasHelmet[MAX_PLAYERS + 1];
	int		m_iArmor[MAX_PLAYERS + 1];
	int		m_iScore[MAX_PLAYERS + 1];
	int		m_iCompetitiveRanking[MAX_PLAYERS + 1];
	int		m_iCompetitiveWins[MAX_PLAYERS + 1];
	int8	m_iCompetitiveRankType[MAX_PLAYERS + 1]; // EPlayerRankTypeId_t
	int		m_iCompTeammateColor[MAX_PLAYERS + 1];
	int		m_iLifetimeStart[MAX_PLAYERS + 1];
	int		m_iLifetimeEnd[MAX_PLAYERS + 1];

#if CS_CONTROLLABLE_BOTS_ENABLED
	bool	m_bControllingBot[MAX_PLAYERS + 1];
	int		m_iControlledPlayer[MAX_PLAYERS + 1];
	int		m_iControlledByPlayer[MAX_PLAYERS + 1];
	char	m_szPlayerNames[MAX_PLAYERS + 1][MAX_PLAYER_NAME_LENGTH];
#endif

	int		m_iBotDifficulty[MAX_PLAYERS + 1];	// Difficulty level of a bot ( -1 if not applicable )
	char	m_szClan[MAX_PLAYERS + 1][MAX_CLAN_TAG_LENGTH];
	int		m_iTotalCashSpent[MAX_PLAYERS + 1];
	int     m_iGunGameLevel[MAX_PLAYERS + 1];
	int		m_iCashSpentThisRound[MAX_PLAYERS + 1];

	int		m_nActiveCoinRank[MAX_PLAYERS + 1];
	int		m_nMusicID[MAX_PLAYERS + 1];

	int m_nPersonaDataPublicLevel[MAX_PLAYERS + 1];
	int m_nPersonaDataPublicCommendsLeader[MAX_PLAYERS + 1];
	int m_nPersonaDataPublicCommendsTeacher[MAX_PLAYERS + 1];
	int m_nPersonaDataPublicCommendsFriendly[MAX_PLAYERS + 1];



public:
	int m_iMatchStats_Kills_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_Assists_Total[ MAX_PLAYERS + 1 ];
	int m_iMatchStats_Deaths_Total[ MAX_PLAYERS + 1 ];

	int m_iMatchStats_3k_Total[ MAX_PLAYERS + 1 ];
	int m_iMatchStats_4k_Total[ MAX_PLAYERS + 1 ];
	int m_iMatchStats_5k_Total[ MAX_PLAYERS + 1 ];

	int m_iMatchStats_Damage_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_EquipmentValue_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_KillReward_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_LiveTime_Total[MAX_PLAYERS + 1];

	int m_iMatchStats_HeadShotKills_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_Objective_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_CashEarned_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_UtilityDamage_Total[MAX_PLAYERS + 1];
	int m_iMatchStats_EnemiesFlashed_Total[MAX_PLAYERS + 1];



};

class C_CS_PlayerResource : public C_PlayerResource, public C_CS_PlayerResourceLatchableGameState
{
	DECLARE_CLASS( C_CS_PlayerResource, C_PlayerResource );
public:
	DECLARE_CLIENTCLASS();

					C_CS_PlayerResource();
	virtual			~C_CS_PlayerResource();

	virtual	void	OnDataChanged(DataUpdateType_t updateType);


	int				GetEndMatchNextMapVote( int iIndex );
	bool			EndMatchNextMapAllVoted( void );

	const wchar_t*		GetDecoratedPlayerName( int name, OUT_Z_BYTECAP(bufflen) wchar_t* buffer, int bufflen, EDecoratedPlayerNameFlag_t flags );

	virtual const char *GetPlayerName( int index ) OVERRIDE;
	char const *SanitizePlayerName( int idxPlayer ) OVERRIDE;
	bool			IsAssassinationTarget( int index );

#if defined ENABLE_CLIENT_INVENTORIES_FOR_OTHER_PLAYERS
	CCSPlayerInventory * GetInventory( int index );
#endif

protected:

	virtual const char*	SetupPlayerName( int slot, char (&scratch)[PLAYER_RESOURCE_SCRATCH_LENGTH] ) OVERRIDE;

	bool	m_bDisableAssassinationTargetNameOverride;
	bool	m_bIsAssassinationTarget[MAX_PLAYERS + 1];

	// Don't latch this state
	int		m_nEndMatchNextMapVotes[MAX_PLAYERS + 1];
	bool	m_bEndMatchNextMapAllVoted;

#if defined ENABLE_CLIENT_INVENTORIES_FOR_OTHER_PLAYERS
	CCSPlayerInventory m_Inventory[ MAX_PLAYERS + 1 ];
#endif
};

FORCEINLINE C_CS_PlayerResource *GetCSResources( void )
{
	return assert_cast< C_CS_PlayerResource* >( g_PR );
}

#endif // C_CS_PLAYERRESOURCE_H
