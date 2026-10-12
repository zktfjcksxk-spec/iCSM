//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CS_APP_LIFETIME_GAMESTATS_H 
#define CS_APP_LIFETIME_GAMESTATS_H 
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"
#include "steam/isteamgamestats.h"
#include "steam/isteamfriends.h"
#include "steamworks_gamestats.h"

#define UI_EVENT_NAME_MAX 80 

// This class manages an OGS session that persists for the lifetime of the client (from program launch to shutdown)
class CS_App_Lifetime_Gamestats : public CSteamWorksGameStatsUploader
{
	DECLARE_CLASS( CS_App_Lifetime_Gamestats, CSteamWorksGameStatsUploader )
public:
	CS_App_Lifetime_Gamestats();

	virtual bool Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;

	void RecordUIEvent( const char* szEventName ); 
	void RecordGameJoin( uint64 ullSessionID );

	virtual EGameStatsAccountType GetGameStatsAccountType()	OVERRIDE { return k_EGameStatsAccountType_Steam; }
	virtual void AddSessionIDsToTable( int iTableID ) OVERRIDE;

	STEAM_CALLBACK( CS_App_Lifetime_Gamestats, OnGameJoinRequested, GameRichPresenceJoinRequested_t, m_GameJoinRequested );
private:
	// Data for rows in the game join table
	static uint16 m_unEventCount;
	struct UIEvent_t 
	{
		RTime32				unTime;
		uint16				unCount;
		CUtlString			strEventID;	
	};

	struct GameJoins_t
	{
		uint64				ullSessionID;
		int					unGameType;
		int					unGameMode;
		bool				bIsValveOfficial;
		int					nCompetitiveRanking;
		bool				bMuteOthers = false;
		bool				bHideAvatars = false;
		bool				bCleanNames = false;

		bool operator== (const GameJoins_t &a) const
		{
			return ullSessionID == a.ullSessionID;
		}
	};

	void WriteStats();
	EResult WriteUIEvents();
	EResult WriteGameJoins();

	CUtlVector<UIEvent_t> m_vecUIEvents;
	CUtlVector<GameJoins_t> m_vecGameJoins;
};

CS_App_Lifetime_Gamestats* CSAppLifetimeGameStats();

#endif 