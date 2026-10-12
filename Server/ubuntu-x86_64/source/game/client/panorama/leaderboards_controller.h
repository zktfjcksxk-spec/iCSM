//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Steam Leaderboards Controller
//=============================================================================//
#pragma once

#include "panorama/uievent.h"
#include "tier1/utlhashmaplarge.h"
#include "steam/steam_api.h"


DECLARE_PANORAMA_EVENT1( SteamLeaderboardUpdated, const char * );

class CLeaderboardDataIteratorProxy;


//-----------------------------------------------------------------------------
// Purpose: Interfaces between Steam leaderboards and Panorama
//-----------------------------------------------------------------------------
class CLeaderboardsController
{
public:
	CLeaderboardsController( bool bGlobal );
	~CLeaderboardsController();

	// Not a singleton, but a doubleton!
	static CLeaderboardsController &GetFriends();
	static CLeaderboardsController &GetGlobal();

	void Initialize();

public:
	void EnsureLeaderboardData( const char *pszLeaderboardName, bool bForceRefresh = false );

	bool HasLeaderboardData( const char *pszLeaderboardName ) const;

	// Insert an artificial entry so that we can always have the local user
	// present, and it can be more up to date than we've actually requested
	// a leaderboard for (so we don't have to refresh it whenever these
	// sorts of things change, because we'd also need a delay etc.)
	// This does get stomped whenever the leaderboard data comes in, so the
	// best pattern is to call this after EnsureLeaderboardData before you
	// start querying about a leaderboard to render the UI.
	void InsertArtificialLeaderboardEntry( const char *pszLeaderboardName, CSteamID steamIDUser, int32 nScore, bool bOverwrite = true );

	// For use via C++11 range-based for loop, see below for example
	CLeaderboardDataIteratorProxy LeaderboardData( const char *pszLeaderboardName ) const;

	bool BGetLeaderboardRankAndSize( const char *pszLeaderboardName, AccountID_t unAccountID, int *pRank, int *pSize ) const;

private:
	void OnFindLeaderboard( LeaderboardFindResult_t *pFindLeaderboardResult, bool bIOFailure );
	CUtlVector< CCallResult<CLeaderboardsController, LeaderboardFindResult_t> * > m_vecFindLeaderboardCallResults;

	void OnLeaderboardDownloadedEntries( LeaderboardScoresDownloaded_t *pLeaderboardScoresDownloaded, bool bIOFailure );

	int LeaderboardDataCount( int iMap ) const;
	const LeaderboardEntry_t & LeaderboardData( int iMap, int iChildIndex ) const;

	bool m_bGlobal;

	struct LeaderboardData_t
	{
		CUtlString strLeaderboardName;
		SteamLeaderboard_t hSteamLeaderboard;
		bool bHasLoadedData;
		CUtlVector<LeaderboardEntry_t> vecEntries;
		CCallResult<CLeaderboardsController, LeaderboardScoresDownloaded_t> callResultDownloadEntries;
	};
	typedef CUtlHashMapLarge<const char *, LeaderboardData_t *, CaseSensitiveStrEquals, MurmurHash3ConstCharPtr> MapLeaderboardNameToData_t;
	MapLeaderboardNameToData_t m_mapLeaderboardNameToData;

	friend class CLeaderboardDataIterator;
	friend class CLeaderboardDataIteratorProxy;
};


//-----------------------------------------------------------------------------
// Purpose: Iterator for the leaderboards result data.
// Example:
//		for ( const LeaderboardEntry_t & entry : CLeaderboardsController::Get().LeaderboardData( pszLeaderboardName ) )
//		{
//			DebugMsg( "Score: %d\n", entry.m_nScore ); 
//		}
//-----------------------------------------------------------------------------
class CLeaderboardDataIterator
{
public:
	CLeaderboardDataIterator( int iMap, int iChildIndex, const CLeaderboardsController *pController ) : m_iMap( iMap ), m_iChildIndex( iChildIndex ), m_pController( pController )
	{
		Assert( iChildIndex >= 0 );
		Assert( ( iMap == -1 ) || ( iChildIndex <= m_pController->LeaderboardDataCount( m_iMap ) ) );
		Assert( pController );
	}

	const LeaderboardEntry_t & operator*() { Assert( m_iMap != -1 ); return m_pController->LeaderboardData( m_iMap, m_iChildIndex ); }
	void operator++() { ++m_iChildIndex; }
	bool operator!=( const CLeaderboardDataIterator &other ) { return m_iMap != other.m_iMap || m_iChildIndex != other.m_iChildIndex; }

private:
	const CLeaderboardsController *m_pController;
	int m_iMap;
	int m_iChildIndex;
};

class CLeaderboardDataIteratorProxy
{
public:
	CLeaderboardDataIterator begin() { return CLeaderboardDataIterator( m_iMap, 0, m_pController ); }
	CLeaderboardDataIterator end() { return CLeaderboardDataIterator( m_iMap, ( m_iMap == -1 ) ? 0 : m_pController->LeaderboardDataCount( m_iMap ), m_pController ); }

private:
	CLeaderboardDataIteratorProxy( int iMap, const CLeaderboardsController *pController ) : m_iMap( iMap ), m_pController( pController )
	{
		// An iMap of -1 means that we're not pointing at anything but we
		// still want to create an iterator (it should just iterate over
		// nothing since begin() == end() in this case.)
	}

	const CLeaderboardsController *m_pController;
	int m_iMap;

	friend class CLeaderboardsController;
};
