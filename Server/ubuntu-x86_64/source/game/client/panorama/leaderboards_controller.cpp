//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "leaderboards_controller.h"
#include "panorama/controls/panel2d.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( SteamLeaderboardUpdated );


/*static*/ CLeaderboardsController &CLeaderboardsController::GetFriends()
{
	static CLeaderboardsController s_controller( false /* !bGlobal */ );
	return s_controller;
}

/*static*/ CLeaderboardsController &CLeaderboardsController::GetGlobal()
{
	static CLeaderboardsController s_controller( true /* bGlobal */ );
	return s_controller;
}

CLeaderboardsController::CLeaderboardsController( bool bGlobal )
	: m_bGlobal( bGlobal )
{

}

CLeaderboardsController::~CLeaderboardsController()
{
	m_vecFindLeaderboardCallResults.PurgeAndDeleteElements();
	m_mapLeaderboardNameToData.PurgeAndDeleteElements();
}

void CLeaderboardsController::Initialize()
{

}

void CLeaderboardsController::OnFindLeaderboard( LeaderboardFindResult_t *pFindLeaderboardResult, bool bIOFailure )
{
	if ( pFindLeaderboardResult->m_bLeaderboardFound && !bIOFailure )
	{
		// Look up the leaderboard name so we can map it back to our structures
		const char *pszLeaderboardName = steamapicontext->SteamUserStats()->GetLeaderboardName( pFindLeaderboardResult->m_hSteamLeaderboard );

		int iMap = m_mapLeaderboardNameToData.Find( pszLeaderboardName );
		if ( iMap != m_mapLeaderboardNameToData.InvalidIndex() )
		{
			LeaderboardData_t *pLeaderboardData = m_mapLeaderboardNameToData[iMap];
			pLeaderboardData->hSteamLeaderboard = pFindLeaderboardResult->m_hSteamLeaderboard;

			// Kick off a data request for the actual entries
			ELeaderboardDataRequest eDataRequest = m_bGlobal ? k_ELeaderboardDataRequestGlobal : k_ELeaderboardDataRequestFriends;
			int nRangeEnd = m_bGlobal ? 100 : 0;
			SteamAPICall_t hSteamAPICall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntries( pLeaderboardData->hSteamLeaderboard, eDataRequest, 0, nRangeEnd );
			pLeaderboardData->callResultDownloadEntries.Set( hSteamAPICall, this, &CLeaderboardsController::OnLeaderboardDownloadedEntries );
		}
	}

	// Clean up any finished callbacks we might have
	FOR_EACH_VEC_BACK( m_vecFindLeaderboardCallResults, i )
	{
		if ( m_vecFindLeaderboardCallResults[i]->IsActive() )
			continue;

		delete m_vecFindLeaderboardCallResults[i];
		m_vecFindLeaderboardCallResults.FastRemove( i );
	}
}

void CLeaderboardsController::OnLeaderboardDownloadedEntries( LeaderboardScoresDownloaded_t *pLeaderboardScoresDownloaded, bool bIOFailure )
{
	// If our request failed, do nothing.
	if ( !pLeaderboardScoresDownloaded || bIOFailure )
		return;

	// Look up this leaderboard, if we don't find it just give up (should never
	// actually happen in practice, but...)
	const char *pszLeaderboardName = steamapicontext->SteamUserStats()->GetLeaderboardName( pLeaderboardScoresDownloaded->m_hSteamLeaderboard );
	int iMap = m_mapLeaderboardNameToData.Find( pszLeaderboardName );
	if ( iMap == m_mapLeaderboardNameToData.InvalidIndex() )
		return;

	LeaderboardData_t *pLeaderboardData = m_mapLeaderboardNameToData[iMap];

	pLeaderboardData->vecEntries.RemoveAll();
	for ( int i = 0; i < pLeaderboardScoresDownloaded->m_cEntryCount; i++ )
	{
		LeaderboardEntry_t *pLeaderboardEntry = pLeaderboardData->vecEntries.AddToTailGetPtr();
		steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( pLeaderboardScoresDownloaded->m_hSteamLeaderboardEntries, i, pLeaderboardEntry, NULL, 0 );
	}

	// Mark our leaderboard data so we know we've received a response (we can't
	// just look at the data vector because we may have gotten no rows back,
	// which is a valid state.)
	pLeaderboardData->bHasLoadedData = true;

	// We've gotten new data, so fire off the associated Panorama event.
	panorama::UIEngine()->DispatchEvent( SteamLeaderboardUpdated::MakeEvent( nullptr, pszLeaderboardName ) );
}

void CLeaderboardsController::EnsureLeaderboardData( const char *pszLeaderboardName, bool bForceRefresh /* = false */ )
{
	// Make sure there's an entry in our map for this leaderboard. If there
	// isn't, then all we need to do is add an entry and dispatch an async
	// request for its handle.
	int iMap = m_mapLeaderboardNameToData.Find( pszLeaderboardName );
	if ( iMap == m_mapLeaderboardNameToData.InvalidIndex() )
	{
		LeaderboardData_t *pNewLeaderboardData = new LeaderboardData_t();
		pNewLeaderboardData->strLeaderboardName = pszLeaderboardName;
		pNewLeaderboardData->hSteamLeaderboard = 0;
		pNewLeaderboardData->bHasLoadedData = false;
		iMap = m_mapLeaderboardNameToData.Insert( pNewLeaderboardData->strLeaderboardName, pNewLeaderboardData );

		// Dispatch a request to find the leaderboard by name.
		SteamAPICall_t hSteamAPICall = steamapicontext->SteamUserStats()->FindLeaderboard( pszLeaderboardName );
		CCallResult<CLeaderboardsController, LeaderboardFindResult_t> *pCallResultFindLeaderboard = new CCallResult<CLeaderboardsController, LeaderboardFindResult_t>();
		pCallResultFindLeaderboard->Set( hSteamAPICall, this, &CLeaderboardsController::OnFindLeaderboard );
		m_vecFindLeaderboardCallResults.AddToTail( pCallResultFindLeaderboard );
		return;
	}

	LeaderboardData_t *pLeaderboardData = m_mapLeaderboardNameToData[iMap];

	// If we don't have a valid handle, then we just need to wait around for it
	// to come down (if it never does, we failed to find the leaderboard and we
	// just let ourselves get stuck in this state.)
	if  ( !pLeaderboardData->hSteamLeaderboard )
		return;

	// If we already had a valid handle, then we don't need to request any of
	// the data unless this is a forced refresh. Eventually we may want to
	// add an expiration to this cache?
	if ( !bForceRefresh )
		return;

	// If have an outstanding request already, don't fire off another one.
	if ( pLeaderboardData->callResultDownloadEntries.IsActive() )
		return;

	ELeaderboardDataRequest eDataRequest = m_bGlobal ? k_ELeaderboardDataRequestGlobal : k_ELeaderboardDataRequestFriends;
	int nRangeEnd = m_bGlobal ? 100 : 0;
	SteamAPICall_t hSteamAPICall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntries( pLeaderboardData->hSteamLeaderboard, eDataRequest, 0, nRangeEnd );
	pLeaderboardData->callResultDownloadEntries.Set( hSteamAPICall, this, &CLeaderboardsController::OnLeaderboardDownloadedEntries );
}

bool CLeaderboardsController::HasLeaderboardData( const char *pszLeaderboardName ) const
{
	// If the leaderboard isn't even in our map, we have no data.
	int iMap = m_mapLeaderboardNameToData.Find( pszLeaderboardName );
	if ( iMap == m_mapLeaderboardNameToData.InvalidIndex() )
		return false;

	return m_mapLeaderboardNameToData[iMap]->bHasLoadedData;
}

void CLeaderboardsController::InsertArtificialLeaderboardEntry( const char *pszLeaderboardName, CSteamID steamIDUser, int32 nScore, bool bOverwrite /* = true */ )
{
	EnsureLeaderboardData( pszLeaderboardName );

	int iMap = m_mapLeaderboardNameToData.Find( pszLeaderboardName );
	Assert( iMap != m_mapLeaderboardNameToData.InvalidIndex() );

	LeaderboardData_t *pLeaderboardData = m_mapLeaderboardNameToData[iMap];

	// First, walk over the entries and remove and we have for this account ID.
	FOR_EACH_VEC( pLeaderboardData->vecEntries, i )
	{
		const LeaderboardEntry_t &entry = pLeaderboardData->vecEntries[i];
		if ( entry.m_steamIDUser != steamIDUser )
			continue;

		// If we're not overwriting, then we just give up when we find a
		// matching entry for this user in this leaderboard.
		if ( !bOverwrite )
			return;

		pLeaderboardData->vecEntries.Remove( i );
		break;
	}

	// Then, find the position we should be in (assume descending... this is
	// kind of dumb, but we don't actually know how it's configured on Steam
	// and it's weird if you have to pass it in as a bool, so let's just
	// genericize and assume they have a descending leaderboard.)
	int iInsert = 0;
	for ( ; iInsert < pLeaderboardData->vecEntries.Count(); iInsert++ )
	{
		const LeaderboardEntry_t &entry = pLeaderboardData->vecEntries[iInsert];
		if ( nScore >= entry.m_nScore )
			break;

		// We're not better than the current one, so continue on.
		continue;
	}

	// Whatever iInsert is, we insert our new record there. It might be at
	// count (which means insert at the end) or it might be before some other
	// element, so we just insert wherever it tells us to.
	int iVec = pLeaderboardData->vecEntries.InsertBefore( iInsert );
	LeaderboardEntry_t &entryNew = pLeaderboardData->vecEntries[iVec];
	entryNew.m_steamIDUser = steamIDUser;
	entryNew.m_nGlobalRank = 0;
	entryNew.m_nScore = nScore;
	entryNew.m_cDetails = 0;
	entryNew.m_hUGC = k_UGCHandleInvalid;
}

CLeaderboardDataIteratorProxy CLeaderboardsController::LeaderboardData( const char *pszLeaderboardName ) const
{
	int iMap = m_mapLeaderboardNameToData.Find( pszLeaderboardName );
	if ( iMap == m_mapLeaderboardNameToData.InvalidIndex() )
		return CLeaderboardDataIteratorProxy( -1, this );

	return CLeaderboardDataIteratorProxy( iMap, this );
}

bool CLeaderboardsController::BGetLeaderboardRankAndSize( const char *pszLeaderboardName, AccountID_t unAccountID, int *pRank, int *pSize ) const
{
	int iMap = m_mapLeaderboardNameToData.Find( pszLeaderboardName );
	if ( iMap == m_mapLeaderboardNameToData.InvalidIndex() )
	{
		*pRank = 0;
		*pSize = 0;
		return false;
	}

	const LeaderboardData_t &leaderboardData = *m_mapLeaderboardNameToData[iMap];

	int iFoundEntry = -1;
	for ( int iLeadboardEntry = 0; iLeadboardEntry < leaderboardData.vecEntries.Count(); iLeadboardEntry++ )
	{
		if ( leaderboardData.vecEntries[iLeadboardEntry].m_steamIDUser.GetAccountID() == unAccountID )
		{
			iFoundEntry = iLeadboardEntry;
			break;
		}
	}


	if ( iFoundEntry == -1 )
	{
		*pSize = leaderboardData.vecEntries.Count() + 1;
		*pRank = *pSize;
	}
	else
	{
		*pSize = leaderboardData.vecEntries.Count();
		*pRank = iFoundEntry + 1;
	}

	return true;
}


int CLeaderboardsController::LeaderboardDataCount( int iMap ) const
{
	Assert( iMap != m_mapLeaderboardNameToData.InvalidIndex() );
	return m_mapLeaderboardNameToData[iMap]->vecEntries.Count();
}

const LeaderboardEntry_t & CLeaderboardsController::LeaderboardData( int iMap, int iChildIndex ) const
{
	Assert( iMap != m_mapLeaderboardNameToData.InvalidIndex() );
	return m_mapLeaderboardNameToData[iMap]->vecEntries[iChildIndex];
}
