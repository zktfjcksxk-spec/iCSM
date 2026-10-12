//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PLAYERRESOURCE_H
#define C_PLAYERRESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "const.h"
#include "c_baseentity.h"
#include <igameresources.h>

#define PLAYER_UNCONNECTED_NAME	"unconnected"
#define PLAYER_ERROR_NAME		"ERRORNAME"

#define PLAYER_RESOURCE_SCRATCH_LENGTH 1024

// Data in here can be copied by individual components to 'latch' it to its current state.
// For example, at end of match, the scoreboard might copy this state so that players disconnecting
// doesn't cause them to drop out of existence.
class C_PlayerResourceLatchableGameState
{
public:
	C_PlayerResourceLatchableGameState();

	// Team data access
	const Color&GetTeamColor( int idxTeam );

	// Player data access
	bool	IsConnected( int idxPlayer );
	bool	IsAlive( int idxPlayer );

	const char *GetPlayerName( int idxPlayer );
	int		GetPing( int idxPlayer );
	//	int		GetPacketloss( int idxPlayer );
	int		GetKills( int idxPlayer );
	int		GetAssists( int idxPlayer );
	int		GetDeaths( int idxPlayer );
	int		GetTeam( int idxPlayer );
	int		GetPendingTeam( int idxPlayer );
	int		GetFrags( int idxPlayer );
	int		GetHealth( int idxPlayer );
	int		GetCoachingTeam( int idxPlayer );

	// Returns the xuid for the given player, but only if they are connected
	XUID			GetXuid( int idxPlayer );
	void			FillXuidText( int iIndex, char *buf, int bufSize );

protected:
	// Data for each player that's propagated to all clients
	// Stored in individual arrays so they can be sent down via datatables
	string_t m_szName[MAX_PLAYERS + 1];
	bool	m_bConnected[MAX_PLAYERS + 1];

	int		m_iPing[MAX_PLAYERS + 1];
	int		m_iKills[MAX_PLAYERS + 1];
	int		m_iAssists[MAX_PLAYERS + 1];
	int		m_iDeaths[MAX_PLAYERS + 1];
	int		m_iTeam[MAX_PLAYERS + 1];
	int		m_iPendingTeam[MAX_PLAYERS + 1];
	bool	m_bAlive[MAX_PLAYERS + 1];
	int		m_iHealth[MAX_PLAYERS + 1];
	int		m_iCoachingTeam[MAX_PLAYERS + 1];
	XUID	m_Xuids[MAX_PLAYERS + 1];

	Color	m_Colors[MAX_TEAMS];
};

class C_PlayerResource : public C_BaseEntity, public IGameResources, public IShaderDeviceDependentObject, public C_PlayerResourceLatchableGameState
{
	DECLARE_CLASS( C_PlayerResource, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_PlayerResource();
	virtual			~C_PlayerResource();

public : // IGameResources interface

	// Team data access 
	virtual int		GetTeamScore( int idxTeam ) OVERRIDE;
	virtual const char *GetTeamName( int idxTeam ) OVERRIDE;

	virtual bool	IsFakePlayer( int idxPlayer ) OVERRIDE;
	virtual bool	IsLocalPlayer( int idxPlayer ) OVERRIDE;
	bool			IsHLTV( int idxPlayer );

	virtual const char* GetPlayerName( int idxPlayer ) OVERRIDE; // some update logic in here, can't blindly forward to latchable state

#define FORWARD_PR_LATCHABLE( typ, fn )							\
	typ fn( int idx ) OVERRIDE									\
	{															\
		 return C_PlayerResourceLatchableGameState::fn( idx );	\
	}

	// IGameResources forwards
	FORWARD_PR_LATCHABLE( bool, IsAlive );
	FORWARD_PR_LATCHABLE( bool, IsConnected );
	// FORWARD_PR_LATCHABLE( const char*, GetPlayerName ); // some update logic in here
	FORWARD_PR_LATCHABLE( int, GetKills );
	FORWARD_PR_LATCHABLE( int, GetPing );
	// FORWARD_PR_LATCHABLE( int, GetPacketLoss );
	FORWARD_PR_LATCHABLE( int, GetDeaths );
	FORWARD_PR_LATCHABLE( int, GetFrags );
	FORWARD_PR_LATCHABLE( int, GetTeam );
	FORWARD_PR_LATCHABLE( int, GetHealth );
	FORWARD_PR_LATCHABLE( const Color&, GetTeamColor );

	virtual void	ClientThink();
	virtual	void	OnDataChanged(DataUpdateType_t updateType);
	virtual void	DeviceLost( void );
	virtual void	DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
	virtual void	ScreenSizeChanged( int width, int height ) { }
	virtual void	TeamChanged( void ){ }

protected:
	void	UpdatePlayerName( int slot );
	void	UpdateXuids( void );

	virtual const char* SetupPlayerName( int slot, char (&scratch)[PLAYER_RESOURCE_SCRATCH_LENGTH] );

	virtual const char* SanitizePlayerName( int idxPlayer );

	const char* SetupLocalizedFakePlayerName( int slot, char const *pchRawPlayerName, char( &scratch )[ PLAYER_RESOURCE_SCRATCH_LENGTH ] );
};

extern C_PlayerResource *g_PR;
char const *Helper_GetFriendPersonaNameSanitized( CSteamID steamID );
bool Helper_ShouldSanitizePlayerName( CSteamID steamID );

#endif // C_PLAYERRESOURCE_H
