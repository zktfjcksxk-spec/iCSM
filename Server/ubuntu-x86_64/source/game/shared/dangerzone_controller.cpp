//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "dangerzone_controller.h"
#include "mathlib/hexes.h"

#ifndef CLIENT_DLL

#include "cs_player.h"
#include "baseentity.h"
#include "entityoutput.h"
#include "convar.h"
#include "func_hostage_rescue.h"
#include "survival_spawn_point.h"
#include "weapon_tablet.h"

#ifdef DEVELOPMENT_ONLY
#include "dt_utlvector_send.h"
#endif // DEVELOPMENT_ONLY

#else

#ifdef DEVELOPMENT_ONLY
#include "dt_utlvector_recv.h"
#endif // DEVELOPMENT_ONLY

#endif

#include "cs_gamerules.h"
#include "env_gascanister_shared.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

CHandle< CDangerZoneController > g_hDangerZone = INVALID_EHANDLE;

#define INVALID_DANGERZONE_INDEX -1

ConVar sv_dz_zone_hex_radius( "sv_dz_zone_hex_radius", "2200", FCVAR_RELEASE | FCVAR_REPLICATED );
ConVar sv_dz_zone_bombdrop_money_reward( "sv_dz_zone_bombdrop_money_reward", "15", FCVAR_REPLICATED | FCVAR_RELEASE, "How many money stacks players are rewarded each danger zone wave" );

DEVELOPMENT_ONLY_CONVAR( dev_dz_zone_speed, 50 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_zone_extra_radius_scale, 2 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_zone_bombdrop_interval, 0.2 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_first_prediction_delay, 60 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_final_zone_radius, 500 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_final_zone_duration, 30 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_final_zone_delay, 30 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_final_zone_speed, 30 );
#ifdef GAME_DLL
ConVar sv_dz_zone_damage( "sv_dz_zone_damage", "1", FCVAR_RELEASE | FCVAR_CHEAT );
#endif


#ifndef CLIENT_DLL
#ifdef DEVELOPMENT_ONLY
void CC_DebugTriggerZone_f( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( !pPlayer )
		return;

	if ( CDangerZoneController *pDangerZone = GetDangerZoneController() )
	{
		CDangerZone *pZone = pDangerZone->FindDangerZoneClosestToPoint( pPlayer->GetAbsOrigin() );
		if ( pZone )
		{
			pZone->SetBombLaunchTime( gpGlobals->curtime, pPlayer->GetAbsOrigin() );
		}
	}
}

static ConCommand debug_trigger_zone( "debug_trigger_zone", CC_DebugTriggerZone_f, "", FCVAR_CHEAT );
#endif
#endif

static float GetTotalLerpTime( float flDist )
{
	return flDist / dev_dz_zone_speed.GetFloat();
}

static struct BombWave_t
{
	int m_nBombCount;
	float m_flTimeBetweenWave;
} s_BombWaves[] =
{
	{ 15,	95.f },
	{ 11,	80.f },
	{ 7,	60.f },
	{ 5,	45.f },
	{ 3,	30.f }
};
COMPILE_TIME_ASSERT( ARRAYSIZE( s_BombWaves ) == NUM_BOMB_WAVE );

CDangerZoneController *GetDangerZoneController( void )
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
		return NULL;

	if ( g_hDangerZone )
	{
		return g_hDangerZone;
	}

#ifndef CLIENT_DLL
	if ( !g_hDangerZone )
	{
		CDangerZoneController *pDangerZone = assert_cast<CDangerZoneController*>(CreateEntityByName( "dangerzone_controller" ));
		if ( pDangerZone )
		{
			pDangerZone->SetAbsOrigin( vec3_origin );
			pDangerZone->SetAbsAngles( vec3_angle );
			DispatchSpawn( pDangerZone );

			// g_hDangerZone gets set to this in constructor
			g_hDangerZone = pDangerZone;

			return pDangerZone;
		}
	}
#endif

	return NULL;
}

IMPLEMENT_NETWORKCLASS_ALIASED( DangerZoneController, DT_DangerZoneController )

BEGIN_NETWORK_TABLE( CDangerZoneController, DT_DangerZoneController )
#ifdef GAME_DLL
SendPropBool( SENDINFO( m_bDangerZoneControllerEnabled ) ),
SendPropFloat( SENDINFO( m_flStartTime ) ),
SendPropFloat( SENDINFO( m_flFinalExpansionTime ) ),
SendPropVector( SENDINFO( m_vecEndGameCircleStart ) ),
SendPropVector( SENDINFO( m_vecEndGameCircleEnd ) ),
SendPropArray3( SENDINFO_ARRAY3( m_DangerZones ), SendPropEHandle( SENDINFO_ARRAY( m_DangerZones ) ) ),
SendPropArray3( SENDINFO_ARRAY3( m_flWaveEndTimes ), SendPropFloat( SENDINFO_ARRAY( m_flWaveEndTimes ) ) ),
SendPropEHandle( SENDINFO( m_hTheFinalZone ) ),
#else
RecvPropBool( RECVINFO( m_bDangerZoneControllerEnabled ) ),
RecvPropFloat( RECVINFO( m_flStartTime ) ),
RecvPropFloat( RECVINFO( m_flFinalExpansionTime ) ),
RecvPropVector( RECVINFO( m_vecEndGameCircleStart ) ),
RecvPropVector( RECVINFO( m_vecEndGameCircleEnd ) ),
RecvPropArray3( RECVINFO_ARRAY( m_DangerZones ), RecvPropEHandle( RECVINFO( m_DangerZones[0] ) ) ),
RecvPropArray3( RECVINFO_ARRAY( m_flWaveEndTimes ), RecvPropFloat( RECVINFO( m_flWaveEndTimes[0] ) ) ),
RecvPropEHandle( RECVINFO( m_hTheFinalZone ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS_ALIASED( dangerzone_controller, DangerZoneController );

#ifndef CLIENT_DLL
BEGIN_DATADESC( CDangerZoneController )
DEFINE_FIELD( m_bDangerZoneControllerEnabled, FIELD_BOOLEAN ),
DEFINE_FIELD( m_flStartTime, FIELD_FLOAT ),
DEFINE_FIELD( m_flFinalExpansionTime, FIELD_FLOAT ),
DEFINE_FIELD( m_vecEndGameCircleStart, FIELD_VECTOR ),
DEFINE_FIELD( m_vecEndGameCircleEnd, FIELD_VECTOR ),
DEFINE_AUTO_ARRAY( m_DangerZones, FIELD_EHANDLE ),
DEFINE_AUTO_ARRAY( m_flWaveEndTimes, FIELD_FLOAT ),
END_DATADESC()
#endif

CDangerZoneController::CDangerZoneController( void )
{
#ifndef CLIENT_DLL
	m_bDangerZoneControllerEnabled = false;
	m_flStartTime = 0.f;
	m_vecEndGameCircleStart = vec3_origin;
	m_vecEndGameCircleEnd = vec3_origin;
	m_flLastDangerZoneDamageTime = 0;
	m_flLastDangerZoneStatusLogged = 0;
	m_bFirstBombWarning = false;
	m_numWaveRewardsGranted = 0;
#endif
}

bool CDangerZoneController::IsMasterDangerZoneEnabled()
{
	return m_bDangerZoneControllerEnabled;
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Precache!
//-----------------------------------------------------------------------------
void CDangerZoneController::Precache( void )
{
	BaseClass::Precache();
	PrecacheScriptSound( "Survival.BeaconGlobal" );
}

void CDangerZoneController::Spawn( void )
{
	Precache();

	Assert( !g_hDangerZone.Get() );
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	const int nHexes = WORLD_HEX_NUM;
	for ( int i = 0; i < nHexes; i++ )
	{
		m_DangerZones.Set( i, INVALID_EHANDLE );
	}			
}

void CDangerZoneController::EnableDangerZone( void )
{
	m_bDangerZoneControllerEnabled = true;
	//m_nFinalZoneIndex = -1;

	m_pGasCanLaunchers.Purge();

	CPointEntity *pLaunchPos = NULL;
	while ( (pLaunchPos = assert_cast< CPointEntity* >( gEntList.FindEntityByClassname( pLaunchPos, "info_gascanister_launchpoint" ))) != NULL )
	{
		m_pGasCanLaunchers.AddToTail( pLaunchPos );
	}

	if ( m_pGasCanLaunchers.Count() == 0 )
	{
		pLaunchPos = assert_cast< CPointEntity* >( CreateEntityByName( "info_gascanister_launchpoint" ) );
		if ( pLaunchPos )
		{
			trace_t trace;
			Vector vecStart = Vector( 0, 0, 5000 );
			UTIL_TraceLine( vecStart, vecStart + Vector( 0, 0, 1 ) * 9000, MASK_NPCWORLDSTATIC,
							this, COLLISION_GROUP_NONE, &trace );

			Vector vecLaunchStart = trace.endpos + Vector( 0, 0, -3000 );

			// hack into the skybox
			//Vector vecLaunchStart = Vector( 1504, 80, -6048 );

			pLaunchPos->SetAbsOrigin( vecLaunchStart );
			string_t iszName = AllocPooledString( "GasCanLauncher" );
			pLaunchPos->SetName( iszName );
			DispatchSpawn( pLaunchPos );

			m_pGasCanLaunchers.AddToTail( pLaunchPos );
		}
	}
	

	for ( int nIdx = 0; nIdx < m_DangerZones.Count(); nIdx++ )
	{
		CDangerZone *pDangerZone = assert_cast< CDangerZone* >( CreateEntityByName( "dangerzone_entity" ) );
		if ( pDangerZone )
		{
			DispatchSpawn( pDangerZone );
			m_DangerZones.Set( nIdx, pDangerZone );
		}
	}

	// loop through all danger zones again to init
	for ( int nIdx = 0; nIdx < m_DangerZones.Count(); nIdx++ )
	{
		const Vector2D& vecPos2D = CSGameRules()->GetSurvivalRules()->GetHexCenter( nIdx );
		Vector vecPos = Vector( vecPos2D.x, vecPos2D.y, 0 );

		// this relies on final zone to be selected
		CDangerZone *pZone = GetDangerZone( nIdx );
		if ( pZone )
		{
			pZone->InitDangerZone( vecPos, nIdx );
		}
	}

	DZ_ConsoleMsg( "Enabling Danger Zone!\n" );
}

void CDangerZoneController::DisableDangerZone( void )
{
	m_bDangerZoneControllerEnabled = false;

	for ( int i = 0; i < m_DangerZones.Count(); i++ )
	{
		CDangerZone *pZone = GetDangerZone( i );
		if ( pZone )
		{
			UTIL_Remove( pZone );
		}
	}
}

void CDangerZoneController::UpdateTheFinalZone()
{
	CUtlVector< CDangerZone* > vecZones;
	for ( int i = 0; i < m_DangerZones.Count(); i++ )
	{
		CDangerZone *pZone = GetDangerZone( i );
		if ( pZone )
		{
			vecZones.AddToTail( pZone );
		}
	}

	// randomly select a hex until we find one that has some item spawn points within some radius from the center of the hex
	CUtlVector< CBaseEntity* > vecSpawnPointsInRegion;
#if 0
	static bool s_bOnce = false;
	if ( !s_bOnce )
	{
		s_bOnce = true;
		int numTotalPossibleEnding = 0, numTotalImpossibleEnding = 0;
		FOR_EACH_VEC( vecZones, iTestZone )
		{
			if ( vecZones[ iTestZone ]->GetPotentialEndGamePoints( &vecSpawnPointsInRegion ) > 0 )
				++numTotalPossibleEnding;
			else
				++numTotalImpossibleEnding;
		}
		DevMsg( "Danger zone waves: %d/%d end zones possible\n", numTotalPossibleEnding, numTotalPossibleEnding + numTotalImpossibleEnding );
	}
#endif
	CDangerZone *pFallbackFinalZone = NULL;
	while ( m_hTheFinalZone.Get() == NULL )
	{
		int iRandomZone = RandomInt( 0, vecZones.Count() - 1 );
		CDangerZone *pZone = vecZones[ iRandomZone ];
		
		bool bCanBeFinalZone = ( pZone && pZone->GetPotentialEndGamePoints( &vecSpawnPointsInRegion ) > 0 );
		if ( bCanBeFinalZone )
		{	// Check if there are sufficient neighbors to qualify for end-game?
			int numGoodNeighbors = 0;
			for ( int iNeighbor = 0; iNeighbor < NEIGHBOR_HEX_COUNT; ++iNeighbor )
			{
				CDangerZone *pNeighbor = pZone->GetNeighbor( iNeighbor );
				if ( pNeighbor && pNeighbor != pZone && pNeighbor->BCanBePotentialEndGameHex() )
					++ numGoodNeighbors;
			}
			bCanBeFinalZone = ( numGoodNeighbors >= s_BombWaves[ ARRAYSIZE( s_BombWaves ) - 1 ].m_nBombCount );
			
			if ( !pFallbackFinalZone )	// In case something is wicked broken, we'll use this zone as fallback
				pFallbackFinalZone = pZone;
		}
		
		if ( bCanBeFinalZone )
			m_hTheFinalZone = pZone;
		else
			vecZones.FastRemove( iRandomZone );
	}
	// Safety catch for the fallback final zone
	if ( m_hTheFinalZone.Get() == NULL )
		m_hTheFinalZone = pFallbackFinalZone;

	// Validate our configuration
	AssertFatal( m_hTheFinalZone.Get() );
	AssertFatal( vecSpawnPointsInRegion.Count() );
	AssertFatal( IPointDZWeaponSpawn::AutoList().Count() );
	
	// pick a random spawn point to start
	m_vecEndGameCircleStart = vecSpawnPointsInRegion.Random()->GetAbsOrigin();
	float flEndGameMoveDist = dev_dz_final_zone_speed.GetFloat() * dev_dz_final_zone_duration.GetFloat();
	float flEndGameMoveDistSqr = Square( flEndGameMoveDist );
	// pick a far random spawn point to move toward
	while ( CBaseEntity *pToEnt = IPointDZWeaponSpawn::AutoList().Random()->GetEntity() )
	{
		if ( pToEnt->GetAbsOrigin().DistToSqr( m_vecEndGameCircleStart.Get() ) > flEndGameMoveDistSqr )
		{
			Vector vecMoveDir = ( pToEnt->GetAbsOrigin() - m_vecEndGameCircleStart.Get() ).Normalized();
			m_vecEndGameCircleEnd = m_vecEndGameCircleStart.Get() + flEndGameMoveDist * vecMoveDir;
			break;
		}
	}
}

void CDangerZoneController::CreateWarningEvent( const char *pszName, float flEventTime, bool bWaveMoney )
{
	CFmtStr strPlaySoundThink( "Danger:WarnSnd_%s", pszName );
	SetContextThink( &CDangerZoneController::PlayWarningSound, flEventTime, strPlaySoundThink.Get() );
	CFmtStr strRocketStartOutputThink( "Danger:Rockets_%s", pszName );
	SetContextThink( &CDangerZoneController::FireRocketStartLaunchOutput, flEventTime, strRocketStartOutputThink.Get() );
	if ( bWaveMoney )
	{
		CFmtStr strRewardBombWaveMoneyThink( "Danger:Money_%s", pszName );
		SetContextThink( &CDangerZoneController::RewardBombWaveMoney, flEventTime, strRewardBombWaveMoneyThink.Get() );
	}
}

void CDangerZoneController::ReverseFloodFillBombWaves()
{
	CUtlVector< CDangerZone* > vecTestZones;
	vecTestZones.AddToTail( m_hTheFinalZone.Get() );

	auto lambdaGetFreeNeighborList = [&]( CUtlVector< CDangerZone* >& vecFreeNeighbors, bool bNeedGoodLandHexes )
	{
		vecFreeNeighbors.RemoveAll();
		FOR_EACH_VEC( vecTestZones, iTest )
		{
			CDangerZone *pTest = vecTestZones[iTest];
			for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
			{
				CDangerZone *pNeighbor = pTest->GetNeighbor( iNeighbor );
				if ( pNeighbor && !pNeighbor->IsBombIncoming() && !IsFinalZone( pNeighbor ) &&
					( bNeedGoodLandHexes == pNeighbor->BCanBePotentialEndGameHex() ) )
				{
					if ( vecFreeNeighbors.Find( pNeighbor ) == vecFreeNeighbors.InvalidIndex() )
					{
						vecFreeNeighbors.AddToTail( pNeighbor );
					}
				}
			}
		}
	};
	
	// reverse flood fill
	for ( int iWave=NUM_BOMB_WAVE-1; iWave>=0; --iWave )
	{
		const int nBombToSpawn = s_BombWaves[iWave].m_nBombCount;

		int iSpawn = 0;
		while ( iSpawn < nBombToSpawn )
		{
			int const nLeftToSpawn = nBombToSpawn - iSpawn;

			CUtlVector< CDangerZone* > vecFreeNeighbors;
			lambdaGetFreeNeighborList( vecFreeNeighbors, true );
			int const numNeededFromOpenOcean = ( nLeftToSpawn - vecFreeNeighbors.Count() );
			if ( numNeededFromOpenOcean > 0 ) // if not enough "good land"
			{	// then we may have to fall back to "empty ocean hexes"
				CUtlVector< CDangerZone* > vecOpenOceanFreeNeighbors;
				lambdaGetFreeNeighborList( vecOpenOceanFreeNeighbors, false );
				if ( vecOpenOceanFreeNeighbors.Count() > numNeededFromOpenOcean )
				{
					vecOpenOceanFreeNeighbors.Shuffle();
					vecOpenOceanFreeNeighbors.RemoveMultipleFromTail( vecOpenOceanFreeNeighbors.Count() - numNeededFromOpenOcean );
				}
				vecFreeNeighbors.AddVectorToTail( vecOpenOceanFreeNeighbors );
			}
			Assert( vecFreeNeighbors.Count() != 0 );

			if ( vecFreeNeighbors.Count() > nLeftToSpawn )
			{
				vecFreeNeighbors.Shuffle();
				vecFreeNeighbors.RemoveMultipleFromTail( vecFreeNeighbors.Count() - nLeftToSpawn );
			}

			FOR_EACH_VEC( vecFreeNeighbors, i )
			{
				CDangerZone *pZone = vecFreeNeighbors[i];
				Assert( pZone->GetWaveID() == -1 );
				pZone->SetBombWave( iWave );
			}

			vecTestZones.AddVectorToTail( vecFreeNeighbors );
			iSpawn += vecFreeNeighbors.Count();
		}
		Assert( iSpawn == nBombToSpawn );
	}
}

void CDangerZoneController::UpdateBombDropOrder()
{
	CUtlVector< CDangerZone* > vecTestZones;

	auto lambdaIsNextToPrevWave = [&]( CDangerZone *pZone ) -> bool
	{
		for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
		{
			CDangerZone *pNeighbor = pZone->GetNeighbor( iNeighbor );
			if ( !pNeighbor )
				return true;

			if ( IsFinalZone( pNeighbor ) )
				continue;

			if ( pNeighbor->GetWaveID() > pZone->GetWaveID() )
				continue;

			if ( pNeighbor->GetWaveID() < pZone->GetWaveID() )
				return true;

			// neighbor is in the prev wave or save wave in prev sub wave
			if ( vecTestZones.Find( pNeighbor ) != vecTestZones.InvalidIndex() )
				return true;
		}

		return false;
	};

	auto lambdaGetSafeZoneCount = [&]( CDangerZone *pZone ) -> int
	{
		int nSafeZone = 0;

		if ( pZone )
		{
			for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
			{
				CDangerZone *pNeighbor = pZone->GetNeighbor( iNeighbor );
				if ( pNeighbor && pNeighbor->GetDropOrder() == 0 )
					nSafeZone++;
			}
		}

		return nSafeZone;
	};

	auto lambdaComputeDropOrder = [&]( CDangerZone *pZone ) -> int
	{
		int nMaxDropOrder = 0;
		for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
		{
			CDangerZone *pNeighbor = pZone->GetNeighbor( iNeighbor );
			if ( pNeighbor && pNeighbor->GetWaveID() ==  pZone->GetWaveID() && pNeighbor->GetDropOrder() > nMaxDropOrder )
				nMaxDropOrder = pNeighbor->GetDropOrder();
		}

		return nMaxDropOrder + 1;
	};

	typedef std::pair< CDangerZone*, int > ZoneDropOrder_t;
	auto lambdaUpdateZoneDropOrder = [&]( const CUtlVector< ZoneDropOrder_t > &vecZoneDropOrder )
	{
		FOR_EACH_VEC( vecZoneDropOrder, iZone )
		{
			const ZoneDropOrder_t &dropOrder = vecZoneDropOrder[ iZone ];
			dropOrder.first->SetDropOrder( dropOrder.second );
		}
	};

	for ( int iWave=0; iWave<NUM_BOMB_WAVE; ++iWave )
	{
		vecTestZones.RemoveAll();

		CUtlVector< CDangerZone* > vecZonesInWave;
		GetZonesFromWaveID( iWave, vecZonesInWave );

		while ( vecZonesInWave.Count() != 0 )
		{
			CUtlVector< CDangerZone* > vecSubWave;
			CUtlVector< ZoneDropOrder_t > vecZoneDropOrder;
			FOR_EACH_VEC_BACK( vecZonesInWave, iZone )
			{
				CDangerZone *pZone = vecZonesInWave[ iZone ];
				if ( lambdaIsNextToPrevWave( pZone ) )
				{
					vecSubWave.AddToTail( pZone );
					vecZoneDropOrder.AddToTail( ZoneDropOrder_t( pZone, lambdaComputeDropOrder( pZone ) ) );

					vecZonesInWave.FastRemove( iZone );
				}
			}
			
			// left with stand alone zones
			// just spawn the rest
			if ( vecSubWave.Count() == 0 )
			{
				FOR_EACH_VEC( vecZonesInWave, iZone )
				{
					CDangerZone *pZone = vecZonesInWave[ iZone ];
					vecSubWave.AddToTail( pZone );
					vecZoneDropOrder.AddToTail( ZoneDropOrder_t( pZone, lambdaComputeDropOrder( pZone ) ) );
				}
				vecZonesInWave.RemoveAll();
			}

			// update drop order
			lambdaUpdateZoneDropOrder( vecZoneDropOrder );

			// check if any zone is completely blocked by neighbor from same subwave
			// recompute drop order for those blockers
			CUtlVector< ZoneDropOrder_t > vecBlockers;
			FOR_EACH_VEC( vecSubWave, iZone )
			{
				CDangerZone *pZone = vecSubWave[ iZone ];
				if ( lambdaGetSafeZoneCount( pZone ) == 0 )
				{
					for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
					{
						CDangerZone *pNeighbor = pZone->GetNeighbor( iNeighbor );
						if ( !pNeighbor )
							continue;

						int iSubWaveIndex = vecSubWave.Find( pNeighbor );
						if ( vecSubWave.IsValidIndex( iSubWaveIndex ) )
						{
							vecBlockers.AddToTail( ZoneDropOrder_t( pNeighbor, lambdaComputeDropOrder( pNeighbor ) ) );
						}
					}
				}
			}
			lambdaUpdateZoneDropOrder( vecBlockers );

			vecTestZones.AddVectorToTail( vecSubWave );
		}
	}
}

void CDangerZoneController::UpdateBombLanding()
{
	// compute bomb landing time and location
	float flLastWaveEndTime = GetStartTime();
	float flHexExpansionTime = GetTotalLerpTime( sv_dz_zone_hex_radius.GetFloat() );

	for ( int iWave = 0; iWave<NUM_BOMB_WAVE; ++iWave )
	{
		CUtlVector< CDangerZone* > vecWaveZones;
		GetZonesFromWaveID( iWave, vecWaveZones );
		// sort zones by max hex
		vecWaveZones.Sort( []( CDangerZone* const* ppZone1, CDangerZone* const* ppZone2 ) -> int
		{
			return ( *ppZone1 )->GetDropOrder() - ( *ppZone2 )->GetDropOrder();
		} );

		const float flWaveStartTime = flLastWaveEndTime + s_BombWaves[iWave].m_flTimeBetweenWave;
		float flLastBombLaunchTime = 0.f;
		FOR_EACH_VEC( vecWaveZones, iZone )
		{
			CDangerZone *pZone = vecWaveZones[ iZone ];

			CUtlVector< Vector > vecAffectingPoints;
			for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
			{
				CDangerZone *pNeighbor = pZone->GetNeighbor( iNeighbor );
				if ( IsFinalZone( pNeighbor ) )
					continue;

				// if neighbor is outside hex grid, that also count as prev wave (imaginary zone)
				if ( !pNeighbor )
				{
					// use the opposite neighbor to find out of bound hex pos
					// if we don't find one, we can just ignore these neighbor because they'll cancel each other out
					CDangerZone *pOppositeNeighbor = pZone->GetOppositeNeighbor( iNeighbor );
					if ( pOppositeNeighbor )
					{
						Vector vecToOutOfBoundNeighborHex = pZone->GetAbsOrigin() - pOppositeNeighbor->GetAbsOrigin();
						vecAffectingPoints.AddToTail( pZone->GetAbsOrigin() + vecToOutOfBoundNeighborHex );
					}
				}
				else if ( pNeighbor->GetWaveID() < pZone->GetWaveID() )
				{
					vecAffectingPoints.AddToTail( pNeighbor->GetAbsOrigin() );
				}
				else if ( pNeighbor->GetWaveID() == pZone->GetWaveID() && pNeighbor->GetDropOrder() < pZone->GetDropOrder() )
				{
					vecAffectingPoints.AddToTail( pNeighbor->GetAbsOrigin() );
				}
			}

			Vector vecBombLandingPos = vec3_origin;
			if ( vecAffectingPoints.Count() > 0 )
			{
#ifdef DEVELOPMENT_ONLY
				pZone->m_vecDirFromPositions.RemoveAll();
				pZone->m_vecDirFromPositions.AddVectorToTail( vecAffectingPoints );
#endif // DEVELOPMENT_ONLY

				FOR_EACH_VEC( vecAffectingPoints, i )
				{
					vecBombLandingPos += vecAffectingPoints[i];
				}
				vecBombLandingPos /= vecAffectingPoints.Count();
				Vector vecBombLandingDir = ( vecBombLandingPos - pZone->GetAbsOrigin() ).Normalized();
				vecBombLandingPos = pZone->GetAbsOrigin() + vecBombLandingDir * sv_dz_zone_hex_radius.GetFloat();
			}
			else
			{
				// TODO: figure out what to do with zones that don't attach to danger zone
				vecBombLandingPos = pZone->GetAbsOrigin();
			}

			float flOffsetTime = flHexExpansionTime * ( pZone->GetDropOrder() - 1 );
			float flBombLaunchTime = flWaveStartTime + dev_dz_zone_bombdrop_interval.GetFloat() * iZone + flOffsetTime;
			pZone->SetBombLaunchTime( flBombLaunchTime, vecBombLandingPos );

			flLastBombLaunchTime = MAX( flLastBombLaunchTime, flBombLaunchTime );
		} // end wave

		// create warning event for each bomb wave
		CreateWarningEvent( CFmtStr( "%d", iWave ), flWaveStartTime, true );

		// set wave end time
		flLastWaveEndTime = flLastBombLaunchTime + CEnvGasCanister::GetGasCanisterFlightTime() + flHexExpansionTime;
		m_flWaveEndTimes.Set( iWave, flLastWaveEndTime );
	}

	// create warning event for final expansion
	float flFinalExpansionTime = GetFinalExpansionStartTime();
	CreateWarningEvent( "final", flFinalExpansionTime, false );
}

void CDangerZoneController::UpdateExtraExpansionTime()
{
	for ( int i=0; i<GetDangerZoneCount(); ++i )
	{
		CDangerZone *pZone = GetDangerZone( i );
		if ( IsFinalZone( pZone ) )
			continue;

		pZone->UpdateExtraRadius();
	}

	// now make final zone neighbors expand to pinch the end point
	Vector2D vecEndGamePos = GetEndGameZoneOrigin().AsVector2D();
	float flMaxRadius = 0.f;
	float flNeighborRadius[ NEIGHBOR_HEX_COUNT ];
	for ( int i=0; i<NEIGHBOR_HEX_COUNT; ++i )
	{
		CDangerZone *pNeighbor = m_hTheFinalZone.Get()->GetNeighbor( i );
		float flRadius = 0.f;
		if ( pNeighbor )
		{
			// end game, all the zones pinch to center of the last hex
			flRadius = pNeighbor->GetAbsOrigin().AsVector2D().DistTo( vecEndGamePos );
			Assert( flRadius > sv_dz_zone_hex_radius.GetFloat() );
			flMaxRadius = MAX( flMaxRadius, flRadius );
		}
		flNeighborRadius[i] = flRadius;
	}
	m_flFinalExpansionTime = ( flMaxRadius - sv_dz_zone_hex_radius.GetFloat() ) / dev_dz_zone_speed.GetFloat();
	Assert( m_flFinalExpansionTime > 0.f );

	// update final zone neighbors extra radius info
	for ( int i=0; i<NEIGHBOR_HEX_COUNT; ++i )
	{
		CDangerZone *pNeighbor = m_hTheFinalZone.Get()->GetNeighbor( i );
		if ( pNeighbor )
		{
			pNeighbor->SetExtraRadius( flNeighborRadius[i], GetFinalExpansionStartTime(), m_flFinalExpansionTime );
		}
	}
}

void CDangerZoneController::PrecomputeDangerZones( void )
{
	Assert( ARRAYSIZE( s_BombWaves ) == m_flWaveEndTimes.Count() );
	Assert( dev_dz_first_prediction_delay.GetFloat() < s_BombWaves[0].m_flTimeBetweenWave ); // make sure we announce the first wave before it starts

#ifdef _DEBUG
	// make sure we're dropping the right number of bombs
	int nDebugNumBombs = 0;
	for ( int i=0; i<NUM_BOMB_WAVE; ++i )
	{
		nDebugNumBombs += s_BombWaves[i].m_nBombCount;
	}
	Assert( nDebugNumBombs == GetDangerZoneCount() - 1 );
#endif // _DEBUG

#if 0 // def _DEBUG
	//
	// Benchmark code for zone distributions
	//
	for ( int j = 0; j < 200; ++ j )
	{
		UpdateTheFinalZone();
		ReverseFloodFillBombWaves();

		for ( int iDZ = 0; iDZ < m_DangerZones.Count(); ++iDZ )
		{
			if ( CDangerZone* zone = GetDangerZone( iDZ ) )
			{
				int iWave = ( zone == m_hTheFinalZone.Get() ) ? NUM_BOMB_WAVE : zone->GetWaveID();
				DevMsg( "%d, %d, %d, %d\n", j, ( int ) zone->GetAbsOrigin().x, ( int ) zone->GetAbsOrigin().y, iWave );
			}
		}

		// Clean-up the assignments
		m_hTheFinalZone = ( CDangerZone * ) NULL;
		for ( int iDZ = 0; iDZ < m_DangerZones.Count(); ++iDZ )
		{
			if ( CDangerZone* zone = GetDangerZone( iDZ ) )
			{
				zone->SetBombWave( -1 );
			}
		}
	}
#endif

	// pick a new final zone
	UpdateTheFinalZone();

	float flStart = Plat_FloatTime();
	ReverseFloodFillBombWaves();
	DevMsg( "SURVIVAL: ReverseFloodFillBombWaves took %f\n", Plat_FloatTime() - flStart );

	flStart = Plat_FloatTime();
	UpdateBombDropOrder();
	DevMsg( "SURVIVAL: UpdateBombDropOrder took %f\n", Plat_FloatTime() - flStart );

	// need to figure out final expand time before this
	flStart = Plat_FloatTime();
	UpdateBombLanding();
	DevMsg( "SURVIVAL: UpdateBombLanding took %f\n", Plat_FloatTime() - flStart );

	flStart = Plat_FloatTime();
	UpdateExtraExpansionTime();
	DevMsg( "SURVIVAL: UpdateExtraExpansionTime took %f\n", Plat_FloatTime() - flStart );

	// log all zone expansion time
	for ( int i=0; i<GetDangerZoneCount(); ++i )
	{
		CDangerZone *pZone = GetDangerZone( i );
		if ( !IsFinalZone( pZone ) )
		{
			pZone->Log();
		}
	}
	UTIL_LogPrintf( "SAFEZONE start: time[ %f ] pos[ %f %f %f ] radius[ %f ] stop: time[ %f ] pos[ %f %f %f ] radius[ %f ]\n",
					GetEndGameStartTime() - GetStartTime(), XYZ( m_vecEndGameCircleStart.Get() ), dev_dz_final_zone_radius.GetFloat(),
					GetEndGameStartTime() + dev_dz_final_zone_duration.GetFloat() - GetStartTime(), XYZ( m_vecEndGameCircleEnd.Get() ), 0.f );

	// log wave end time
	for ( int i=0; i<NUM_BOMB_WAVE; ++i )
	{
		UTIL_LogPrintf( "DANGERZONE Wave [ %d ] end time: %f\n", i, m_flWaveEndTimes[i] - GetStartTime() );
	}

#ifdef DEVELOPMENT_ONLY
	KeyValuesAD kvZones( "DANGERZONES" );
	kvZones->SetInt( "final", m_hTheFinalZone.Get()->GetZoneIndex() );
	kvZones->SetString( "endgame_startpos", CFmtStr( "%f %f %f", XYZ( m_vecEndGameCircleStart.Get() ) ) );
	kvZones->SetString( "endgame_endpos", CFmtStr( "%f %f %f", XYZ( m_vecEndGameCircleEnd.Get() ) ) );

	for ( int i=0; i<GetDangerZoneCount(); ++i )
	{
		CDangerZone *pZone = GetDangerZone( i );
		if ( !IsFinalZone( pZone ) )
		{
			KeyValues *pZoneKV = new KeyValues( CFmtStr( "%d", i ) );
			pZoneKV->SetInt( "wave", pZone->GetWaveID() );
			kvZones->AddSubKey( pZoneKV );
		}
	}
	// always do it even with developer 0
	KeyValuesDumpAsDevMsg( kvZones, 0, 0 );
#endif // DEVELOPMENT_ONLY
}

void CDangerZoneController::ResetMasterDangerZone( void )
{
	// There should be item spawn points in map
	Assert( IPointDZWeaponSpawn::AutoList().Count() > 0 );

	m_bFirstBombWarning = false;
	m_numWaveRewardsGranted = 0;

	// update start and end time
	m_flStartTime = gpGlobals->curtime;

	// enable an initialize all of the danger zone ents
	EnableDangerZone();

	// precompute waves of danger zones after they're initialized
	float flStart = Plat_FloatTime();
	PrecomputeDangerZones();
	DevMsg( "SURVIVAL: PrecomputeDangerZones took %f\n", Plat_FloatTime() - flStart );
}

CPointEntity *CDangerZoneController::GetGasCanLaunchPosition( const Vector& vecStartPos )
{
	if ( m_pGasCanLaunchers.Count() <= 0 )
		return NULL;

	CPointEntity *pBestPoint = m_pGasCanLaunchers[0];
	for ( int i = 1; i < m_pGasCanLaunchers.Count(); i++ )
	{
		if ( vecStartPos.AsVector2D().DistTo( m_pGasCanLaunchers[i]->GetAbsOrigin().AsVector2D() ) < vecStartPos.AsVector2D().DistTo( pBestPoint->GetAbsOrigin().AsVector2D() ) )
			pBestPoint = m_pGasCanLaunchers[i];
	}

	return pBestPoint;
}

Vector CDangerZoneController::IteratePointOutOfDangerZones( const Vector& vecPos )
{
	// attempt to iteratively solve an arbitrary point out of the danger zone. NOTE: assumes incoming point is NOT in safe area already

	Vector2D vecPos2D = vecPos.AsVector2D();

	float flClosestDistToSafeZone = 0.0f;
	int nClosestIdxOfSafeZone = -1;
	Vector2D vecNearestSafeZoneCenter = Vector2D( 0, 0 );
	for ( int i = 0; i < m_DangerZones.Count(); i++ )
	{
		CDangerZone* zone = GetDangerZone( i );
		if ( zone )
		{
			if ( zone->GetZoneStartTime() > gpGlobals->curtime )
			{
				Vector vecCenter = zone->GetDangerZoneOrigin( 9999.0f );
				float flDist = vecCenter.DistToSqr( vecPos );
				if ( flDist < flClosestDistToSafeZone || nClosestIdxOfSafeZone == -1 )
				{
					flClosestDistToSafeZone = flDist;
					nClosestIdxOfSafeZone = i;
					vecNearestSafeZoneCenter = vecCenter.AsVector2D();
				}
			}
		}
	}

	float flNudgeToSafeScale = 0.01f;

	for ( int n = 0; n < 5; n++ )
	{
		bool bDidPush = false;

		for ( int i = 0; i < m_DangerZones.Count(); i++ )
		{
			CDangerZone* zone = GetDangerZone( i );
			if ( zone && zone->IsDangerZoneEnabled() )
			{
				float flRadius = zone->GetDangerZoneRadius();
				Vector2D vecZoneOrigin = zone->GetDangerZoneOrigin().AsVector2D();
				Vector2D vecDelta = vecPos2D - vecZoneOrigin;

				if ( vecDelta.LengthSqr() <= (flRadius*flRadius) )
				{
					// we're inside this zone. Push out:
					vecDelta.NormalizeInPlace();

					if ( nClosestIdxOfSafeZone >= 0 )
					{
						Vector2D vecToSafe = vecNearestSafeZoneCenter - vecPos2D;
						vecToSafe.NormalizeInPlace();
						flNudgeToSafeScale = MIN( flNudgeToSafeScale + 0.01f, 0.5f );
						vecDelta += vecToSafe * flNudgeToSafeScale;
						vecDelta.NormalizeInPlace();
					}

					Vector2D vecNewPos = vecZoneOrigin + (vecDelta * flRadius * 1.01f);

					//debugoverlay->AddLineOverlay( Vector( vecPos2D.x, vecPos2D.y, 0 ), Vector( vecNewPos.x, vecNewPos.y, 0 ), 255, 0, 0, true, 0.75f );
					//debugoverlay->AddLineOverlay( Vector( vecZoneOrigin.x, vecZoneOrigin.y, 0 ), Vector( vecPos2D.x, vecPos2D.y, 0 ), 0, 255, 0, true, 0.75f );

					vecPos2D = vecNewPos;

					bDidPush = true;
				}
			}
		}

		if ( !bDidPush )
			break;
	}

	return Vector( vecPos2D.x, vecPos2D.y, vecPos.z ); // preserve z?
}

void CDangerZoneController::ApplyDamageToPlayers( void )
{
	if ( !sv_dz_zone_damage.GetBool() )
		return;

	// perform this operation every 1 second
	static const float kflDangerZoneDamageInterval = 1.0f;
	if ( gpGlobals->curtime - m_flLastDangerZoneDamageTime < kflDangerZoneDamageInterval )
		return;
	m_flLastDangerZoneDamageTime = gpGlobals->curtime;

	for ( int iPlayer = 1; iPlayer <= MAX_PLAYERS; ++iPlayer )
	{
		CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		if ( IsWithinPlayArea( pPlayer->GetAbsOrigin() ) )
		{
			pPlayer->m_nNumDangerZoneDamageHits = 0;
			continue;
		}

		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_ReturnToPlayArea" ); // tell the player it's bad times up in here

		// try to find the closest safe spot
		Vector vecSpeculativeSafePoint = IteratePointOutOfDangerZones( pPlayer->GetAbsOrigin() );
		
		static const float kFlMaxZoneDepth = 2000.0f;
		float flDepthIntoTheZone = kFlMaxZoneDepth;

		if ( IsWithinPlayArea( vecSpeculativeSafePoint ) )
		{
			Vector vecPlayer = pPlayer->GetAbsOrigin();
			vecPlayer.z = 0;
			vecSpeculativeSafePoint.z = 0;
			flDepthIntoTheZone = MIN( flDepthIntoTheZone, vecPlayer.DistTo( vecSpeculativeSafePoint ) );
		}
		
		// if the distance to the border of the end-game circle is closer, use that
		{
			float flDistToEndGameZoneBorder = GetEndGameZoneOrigin().AsVector2D().DistTo( pPlayer->GetAbsOrigin().AsVector2D() ) - GetEndGameZoneRadius();
			flDepthIntoTheZone = MIN( flDepthIntoTheZone, flDistToEndGameZoneBorder );
		}

		// What we really want to do is determine how long a player can survive in the zone at varying depth.
		// So let's set some bounds and back-solve the damage:
		// When you're barely in the zone at all, it should take a 100hp player a full 30 seconds to die. This is 3.33 dps.
		// When you're at max depth in the zone, it should take a 100hp player only 10 seconds to die. This is 10.0dps.

		// This means that the damage you take according to depth should be something like:
		const float flDamageFromDepth = RemapValClamped( flDepthIntoTheZone, 0.0f, kFlMaxZoneDepth, 3.0f, 10.0f );

		// But we also hit you a little harder each hit, to a point, and that accelerates the times above.
		// Let's say that starting after the 3rd hit, we start scaling damage up, capping at 1.5x:
		const float flSubsequentHitMultiplier = 1.0f; // RemapValClamped( (float)pPlayer->m_nNumDangerZoneDamageHits, 3.0f, 5.0f, 1.0f, 1.5f );

		// Lastly int conversion:
		int nDamageToDeal = flDamageFromDepth * flSubsequentHitMultiplier;

#ifdef _DEBUG
		// debug
		float flSecondsUntilDeath = (float)pPlayer->GetHealth() / (float)nDamageToDeal;
		DevMsg( "Player is %.1f deep in zone. Hit number %d is dealing %d damage. %.1f Seconds until death.\n", flDepthIntoTheZone, pPlayer->m_nNumDangerZoneDamageHits, nDamageToDeal, flSecondsUntilDeath );
#endif
		
		// increment hit count on this player
		pPlayer->m_nNumDangerZoneDamageHits++;

		// actually deal the damage
		CTakeDamageInfo dmgDangerZone( this, this, nDamageToDeal, DMG_DANGERZONE );
		pPlayer->TakeDamage( dmgDangerZone );
		pPlayer->m_nDangerZoneDamage += nDamageToDeal;

	}
}


extern ConVar tablet_zone_prediction_time_standard;
void CDangerZoneController::UpdateDangerZoneController( void )
{
	if ( !m_bDangerZoneControllerEnabled )
		return;

	ApplyDamageToPlayers();

	// broadcast first predicted bomb wave to players
	if ( !m_bFirstBombWarning && ShouldShowZonePrediction() )
	{
		m_bFirstBombWarning = true;

		UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SurvivalWarning_FirstBombIncoming" );
		if ( IGameEvent *event = gameeventmanager->CreateEvent( "firstbombs_incoming_warning" ) )
		{
			event->SetBool( "global", true ); // don't really need the userid - the event itself triggers the sound
			gameeventmanager->FireEvent( event );
		}
	}
}

void CDangerZoneController::PlayHornSound()
{
	// play the horn
	if ( m_pGasCanLaunchers.Count() > 0 )
	{
		CPASAttenuationFilter filter( this, ATTN_NONE );
		EmitSound( filter, m_pGasCanLaunchers[0]->entindex(), "Survival.BeaconGlobal" );
	}
}

void CDangerZoneController::RewardBombWaveMoney()
{
	//
	// Give every player that is alive money for surviving this far
	//
	++m_numWaveRewardsGranted;
	for ( int iPlayer = 1; iPlayer <= MAX_PLAYERS; ++iPlayer )
	{
		CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		extern ConVar sv_dz_cash_bundle_size;
		int reward = sv_dz_zone_bombdrop_money_reward.GetInt() * sv_dz_cash_bundle_size.GetInt();
		pPlayer->AddAccount( reward, true, "bomb_wave" );

		if ( CTablet* pTablet = dynamic_cast<CTablet*>( pPlayer->Weapon_OwnsThisType( "weapon_tablet" ) ) )
		{
			pTablet->PushTabletNotification( TABLET_NOTIFICATION_BOMBWAVE_MONEY );
		}
	}
}

void CDangerZoneController::PlayWarningSound()
{
	PlayHornSound();

	CGameSurvivalLogic *pSurvivalLogic = CSGameRules()->GetSurvivalRules()->GetGameSurvivalLogicEntity();
	if ( pSurvivalLogic )
	{
		pSurvivalLogic->m_OnZoneRocketWarning.FireOutput( NULL, NULL );
	}
}

void CDangerZoneController::FireRocketStartLaunchOutput()
{
	CGameSurvivalLogic *pSurvivalLogic = CSGameRules()->GetSurvivalRules()->GetGameSurvivalLogicEntity();
	if ( pSurvivalLogic )
		pSurvivalLogic->m_OnZoneRocketStartLaunch.FireOutput( NULL, NULL );
}

#endif // !CLIENT_DLL

#ifdef CLIENT_DLL
void CDangerZoneController::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		g_hDangerZone = this;
	}
}
#endif // CLIENT_DLL

CDangerZone *CDangerZoneController::GetDangerZone( int nIndex )
{
	if ( nIndex < 0 || nIndex >= WORLD_HEX_NUM )
		return NULL;

	return m_DangerZones[ nIndex ].Get();
}

CDangerZone *CDangerZoneController::FindDangerZoneClosestToPoint( const Vector& vecPos )
{
	CDangerZone* pClosestZone = NULL;

	float flBestDist = FLT_MAX;
	for ( int i = 0; i < m_DangerZones.Count(); i++ )
	{
		CDangerZone* zone = GetDangerZone( i );
		if ( zone )
		{
			float flDist = vecPos.DistToSqr( zone->GetDangerZoneOrigin() );
			if ( flDist < flBestDist )
			{
				flBestDist = flDist;
				pClosestZone = zone;
			}
		}
	}

	return pClosestZone;
}

int CDangerZoneController::GetNextBombWaveIndex() const
{
	for ( int i=0; i<NUM_BOMB_WAVE; ++i )
	{
		if ( gpGlobals->curtime <= m_flWaveEndTimes[i] )
		{
			return i;
		}
	}

	return NUM_BOMB_WAVE;
}

float CDangerZoneController::GetTimeUntilNextWave() const
{
	if ( !ShouldShowZonePrediction() )
		return 0.f;

	float flLastEndWaveTime = GetStartTime();
	for ( int i=0; i<NUM_BOMB_WAVE; ++i )
	{
		float flWaveStartTime = flLastEndWaveTime + s_BombWaves[i].m_flTimeBetweenWave;
		float flTime = flWaveStartTime - gpGlobals->curtime;
		if ( gpGlobals->curtime > flLastEndWaveTime && flTime > 0.f )
		{
			return flTime;
		}
		flLastEndWaveTime = m_flWaveEndTimes[ i ];
	}

	return 0.f;
}

bool CDangerZoneController::ShouldShowZonePrediction() const
{
	return gpGlobals->curtime - m_flStartTime > dev_dz_first_prediction_delay.GetFloat();
}

float CDangerZoneController::GetGameTimeOfWaveEnd( int iWave ) const
{
	if ( iWave < 0 || iWave >= NUM_BOMB_WAVE ) return 0.0f;
	return m_flWaveEndTimes[ iWave ];
}

float CDangerZoneController::GetFinalExpansionStartTime() const
{
	return m_flWaveEndTimes[ NUM_BOMB_WAVE - 1 ] + dev_dz_final_zone_delay.GetFloat();
}

float CDangerZoneController::GetEndGameStartTime() const
{
	return GetFinalExpansionStartTime() + m_flFinalExpansionTime;
}

void CDangerZoneController::GetZonesFromWaveID( int iWave, CUtlVector< CDangerZone* > &vecZones )
{
	for ( int iZone=0; iZone<GetDangerZoneCount(); ++iZone )
	{
		CDangerZone *pZone = GetDangerZone( iZone );
		if ( pZone && !IsFinalZone( pZone ) && pZone->GetWaveID() == iWave )
		{
			vecZones.AddToTail( pZone );
		}
	}
}

float CDangerZoneController::GetDistanceFromDangerZone( const Vector& vecPos, Vector* pZoneCenter /* =nullptr */, float flAddTime /*=0.0f*/ )
{
	float flBestDist = -100000.0f;
	for ( int i = 0; i < m_DangerZones.Count(); i++ )
	{
		CDangerZone* zone = GetDangerZone( i );
		if ( zone && zone->IsDangerZoneEnabled() )
		{
			float flDistInside = zone->DistanceInsideDangerZone( vecPos, flAddTime );
			if ( flDistInside > flBestDist )
			{
				flBestDist = flDistInside;

				if ( pZoneCenter != nullptr )
				{
					*pZoneCenter = zone->GetDangerZoneOrigin();
				}
			}
		}
	}

	return -flBestDist;
}


float CDangerZoneController::GetMyDangerZoneRadius( const Vector& vecPos, float flAddTime /* = 0.0f */ )
{
	//int nClosestIndex = -1;
	float flBestDist = 0;

	for ( int i = 0; i < m_DangerZones.Count(); i++ )
	{
		CDangerZone* zone = GetDangerZone( i );
		if ( zone && zone->IsDangerZoneEnabled() )
		{
			float flDistInside = zone->DistanceInsideDangerZone( vecPos, flAddTime );
			if ( flDistInside > flBestDist )
			{
				flBestDist = flDistInside;
				//nClosestIndex = i;
			}
		}
	}

	return flBestDist;
}

Vector CDangerZoneController::GetEndGameZoneOrigin( float flAddTime /*= 0.f*/ )
{
	const float flEndGameStartTime = GetEndGameStartTime();
	float flLerp = RemapValClamped( gpGlobals->curtime + flAddTime, flEndGameStartTime, flEndGameStartTime + dev_dz_final_zone_duration.GetFloat(), 0.f, 1.f );
	flLerp = clamp( Bias( flLerp, 0.2f ), 0.0f, 1.0f );
	return Lerp( flLerp, m_vecEndGameCircleStart.Get(), m_vecEndGameCircleEnd.Get() );
}

float CDangerZoneController::GetEndGameZoneRadius( float flAddTime /*= 0.f*/ ) const
{
	const float flEndGameStartTime = GetEndGameStartTime();
	float flLerp = RemapValClamped( gpGlobals->curtime + flAddTime, flEndGameStartTime, flEndGameStartTime + dev_dz_final_zone_duration.GetFloat(), 0.f, 1.f );
	return ( 1.f - flLerp ) * dev_dz_final_zone_radius.GetFloat();
}

bool CDangerZoneController::IsWithinPlayArea( const Vector& vecPos, float flRatio /* = 1.0f */, float flAddTime /* = 0.0f */ )
{
	if ( !vecPos.IsValid() )
	{
		Assert( false );
		return true;
	}

	float flInsideZone = GetMyDangerZoneRadius( vecPos, flAddTime /* = 0.0f */ );
	bool bIsInPlayArea = flInsideZone == 0.f;
	// if we're in danger zone, check if we're inside the end game circle
	if ( !bIsInPlayArea )
	{
		bIsInPlayArea |= GetEndGameZoneOrigin( flAddTime ).AsVector2D().DistToSqr( vecPos.AsVector2D() ) < Square( GetEndGameZoneRadius( flAddTime ) );
	}
	return bIsInPlayArea;
}

float CDangerZoneController::DistanceOutsidePlayArea( const Vector& vecPos, float flAddTime /* = 0.0f */ )
{
	if ( !vecPos.IsValid() )
	{
		Assert( false );
		return 0;
	}

	int nIndex = CSGameRules()->GetSurvivalRules()->FindGridCenterIndexClosestToWorldPos( vecPos );
	CDangerZone* zone = GetDangerZone( nIndex );
	if ( !zone || !zone->IsDangerZoneEnabled() )
		return 0;

	return vecPos.AsVector2D().DistTo( zone->GetDangerZoneOrigin().AsVector2D() ) - zone->GetDangerZoneRadius( flAddTime );
}

Vector CDangerZoneController::MovePointIntoPlayArea( const Vector& vecPos, float flBiasToCenter /* = 0.5f */, float flAddTime /* = 0.0f */ )
{
	if ( IsWithinPlayArea( vecPos, 1.0f, flAddTime ) )
		return vecPos;

	// get the closest grid point that is not enabled
	Vector vecClosestHex = Vector( 0, 0, 0 );
	for ( int i = 0; i < m_DangerZones.Count(); i++ )
	{
		CDangerZone* zone = GetDangerZone( i );
		if ( zone && zone->IsDangerZoneEnabled() == false )
		{
			Vector2D vecPos2D = vecPos.AsVector2D();
			const Vector& vecZoneOrigin = zone->GetDangerZoneOrigin( flAddTime );
			if ( vecPos2D.DistToSqr( vecZoneOrigin.AsVector2D() ) < vecPos2D.DistToSqr( vecClosestHex.AsVector2D() ) )
			{
				vecClosestHex = vecZoneOrigin;
			}
		}
	}

	Vector vecSpot = vecClosestHex + ( (vecPos - vecClosestHex).Normalized() * ( sv_dz_zone_hex_radius.GetFloat() - 100.f ) );

	return vecSpot;
}

IMPLEMENT_NETWORKCLASS_ALIASED( DangerZone, DT_DangerZone )

#define MAX_FROM_POS 100

BEGIN_NETWORK_TABLE( CDangerZone, DT_DangerZone )
#ifdef GAME_DLL
SendPropVector( SENDINFO( m_vecDangerZoneOriginStartedAt ) ),
SendPropFloat( SENDINFO( m_flBombLaunchTime ) ),
SendPropFloat( SENDINFO( m_flExtraRadius ) ),
SendPropFloat( SENDINFO( m_flExtraRadiusStartTime ) ),
SendPropFloat( SENDINFO( m_flExtraRadiusTotalLerpTime ) ),
SendPropInt( SENDINFO( m_nDropOrder ) ),
SendPropInt( SENDINFO( m_iWave ) ),

#else
RecvPropVector( RECVINFO( m_vecDangerZoneOriginStartedAt ) ),
RecvPropFloat( RECVINFO( m_flBombLaunchTime ) ),
RecvPropFloat( RECVINFO( m_flExtraRadius ) ),
RecvPropFloat( RECVINFO( m_flExtraRadiusStartTime ) ),
RecvPropFloat( RECVINFO( m_flExtraRadiusTotalLerpTime ) ),
RecvPropInt( RECVINFO( m_nDropOrder ) ),
RecvPropInt( RECVINFO( m_iWave ) ),
#endif
END_NETWORK_TABLE()

#ifdef GAME_DLL
BEGIN_DATADESC( CDangerZone )
END_DATADESC()

LINK_ENTITY_TO_CLASS( dangerzone_entity, CDangerZone );
#endif // GAME_DLL

CDangerZone::CDangerZone( void )
{
#ifndef CLIENT_DLL
	m_flBombLaunchTime = 0.f;
	m_vecDangerZoneOriginStartedAt = vec3_origin;
	m_flExtraRadius = m_flExtraRadiusStartTime = m_flExtraRadiusTotalLerpTime = 0.f;
	m_nDropOrder = 0;
	m_iWave = -1;
	m_nMyZoneIndex = 0;

	m_eZoneHexType = k_EZoneHexType_Unknown;

	for ( int i = 0; i < NEIGHBOR_HEX_COUNT; i++ )
		m_nZoneNeighbors[i] = INVALID_DANGERZONE_INDEX;
#endif
}

bool CDangerZone::IsDangerZoneEnabled( void ) const
{
	if ( !IsBombIncoming() )
		return false;

	return gpGlobals->curtime >= GetZoneStartTime();
}

float CDangerZone::GetDangerZoneLerp( float flAddTime /* = 0.0f */ )
{
	if ( !IsDangerZoneEnabled() )
		return 0.f;

	float flLerp = RemapValClamped( ( gpGlobals->curtime - GetZoneStartTime() ) + flAddTime, 0.f, GetTotalLerpTime( sv_dz_zone_hex_radius.GetFloat() ), 0.f, 1.f );

	return flLerp;
}

Vector CDangerZone::GetDangerZoneOrigin( float flAddTime /*= 0.0f*/ )
{
	float flLerp = GetDangerZoneLerp( flAddTime );
	return Lerp( flLerp, m_vecDangerZoneOriginStartedAt.Get(), GetAbsOrigin() );
}

float CDangerZone::GetDangerZoneRadius( float flAddTime /* = 0.0f */ )
{
	if ( gpGlobals->curtime + flAddTime >= m_flExtraRadiusStartTime )
	{
		if ( !IsDangerZoneEnabled() )
			return 0.f;

		// expand hexes surrounding the final zone to finish up the game
		// offset the curtime to make sure everything collapse withing end game duration
		float flLerp = RemapValClamped( ( gpGlobals->curtime + flAddTime ) - m_flExtraRadiusStartTime, 0.f, m_flExtraRadiusTotalLerpTime, 0.f, 1.f );
		return Lerp( flLerp, sv_dz_zone_hex_radius.GetFloat(), m_flExtraRadius.Get() );
	}

	return Lerp( GetDangerZoneLerp( flAddTime ), 0.f, sv_dz_zone_hex_radius.GetFloat() );
}

float CDangerZone::GetPredictedWaveRadius( int iAddWave /*= 0*/ )
{
	if ( !GetDangerZoneController()->ShouldShowZonePrediction() )
		return 0.f;

	if ( IsBombIncoming() )
	{
		int iWaveIndex = GetDangerZoneController()->GetNextBombWaveIndex();
		int iWave = iWaveIndex + iAddWave;
		if ( iWaveIndex >= NUM_BOMB_WAVE )
		{
			return 0.f;
		}

		if ( iWave >= m_iWave )
		{
			return sv_dz_zone_hex_radius.GetFloat();
		}
	}
	
	return 0.f;
}

bool CDangerZone::IsWithinPlayArea( const Vector& vecPos, float flAddTime /* = 0.0f */ )
{
	if ( !IsDangerZoneEnabled() )
		return true;

	if ( !vecPos.IsValid() )
	{
		Assert( false );
		return true;
	}

	float flRadius = GetDangerZoneRadius( flAddTime );

	if ( flRadius <= 0 )
		return true;

	return (DistanceInsideDangerZone( vecPos, flAddTime ) <= 0);
}

float CDangerZone::DistanceInsideDangerZone( const Vector& vecPos, float flAddTime /* = 0.0f */ )
{
	if ( !IsDangerZoneEnabled() )
		return 0;

	float flDistFromCenter = vecPos.AsVector2D().DistTo( GetDangerZoneOrigin( flAddTime ).AsVector2D() );

	return GetDangerZoneRadius( flAddTime ) - flDistFromCenter;
}

bool CDangerZone::IsBombInflight() const
{
	return gpGlobals->curtime >= m_flBombLaunchTime && gpGlobals->curtime < GetZoneStartTime();
}

float CDangerZone::GetZoneStartTime() const
{
	return m_flBombLaunchTime + CEnvGasCanister::GetGasCanisterFlightTime();
}

Vector CDangerZone::MovePointIntoPlayArea( const Vector& vecPos, float flBiasToCenter /* = 0.5f */, float flAddTime /* = 0.0f */ )
{
	if ( IsWithinPlayArea( vecPos, flAddTime ) )
		return vecPos;

	Vector vecDangerZoneOrigin = GetDangerZoneOrigin( flAddTime );
	return vecDangerZoneOrigin - ((vecPos - vecDangerZoneOrigin).Normalized() * GetDangerZoneRadius( flAddTime ) * flBiasToCenter);
}

#ifndef CLIENT_DLL

void CDangerZone::SpawnDangerZoneCanister()
{
	Assert( !IsDangerZoneEnabled() && IsBombIncoming() );

	// set the midpoint
	{
		const Vector& vecStart = m_vecDangerZoneOriginStartedAt;

/*
 		if ( sv_dz_debug.GetBool() )
 		{
			const Vector& vecEnd = GetAbsOrigin();
			Vector vecMid = Lerp( 0.5f, vecStart, vecEnd );
			Vector vecOrtho = CrossProduct( ( vecEnd - vecStart ).Normalized(), Vector( 0, 0, 1 ) );

			// push the midpoint orthogonally by a quarter of the distance
			vecMid += vecOrtho * vecStart.DistTo( vecEnd ) * 0.25f;
		
 			debugoverlay->AddLineOverlay( vecStart, vecEnd, 255, 255, 0, true, 40.0f );
 			debugoverlay->AddLineOverlay( vecStart, vecMid, 0, 255, 0, true, 40.0f );
 			debugoverlay->AddLineOverlay( vecMid, vecEnd, 0, 255, 0, true, 40.0f );
		}
*/

		CEnvGasCanister *pGasCanister = ( CEnvGasCanister* )CreateEntityByName( "env_gascanister" );
		if ( pGasCanister )
		{
			Vector vecUp;
			GetVectors( NULL, NULL, &vecUp );
			QAngle angFacing;
			VectorAngles( vecUp, angFacing );

			trace_t trace;
			UTIL_TraceLine( vecStart + vecUp * 2000, vecStart, MASK_NPCWORLDSTATIC,
							this, COLLISION_GROUP_NONE, &trace );

			const Vector& vecGasStart = trace.endpos;

			//void InitInWorld( float flLaunchTime, const Vector &vecStartPosition, const QAngle &vecStartAngles, const Vector &vecDirection, const Vector &vecImpactPosition, bool bLaunchedFromWithinWorld = false );
			pGasCanister->SetAbsOrigin( vecGasStart );
			pGasCanister->SetAbsAngles( angFacing );

			pGasCanister->m_nMyZoneIndex = m_nMyZoneIndex;
			DispatchSpawn( pGasCanister );
			pGasCanister->AcceptInput( "FireCanister", this, this, variant_t(), 0 );
			
			if ( trace.startsolid || trace.allsolid || ( trace.DidHit() && trace.surface.flags & SURF_SKY ) )
			{
				pGasCanister->DisableImpactEffects();
			}
		}
	}
}

void CDangerZone::InitDangerZone( const Vector& vecPos, int nIdx )
{
	SetAbsOrigin( vecPos );
	SetAbsAngles( vec3_angle );
	m_nMyZoneIndex = nIdx;

	// give each hex awareness of it's neighbors
	int zX = m_nMyZoneIndex % WORLD_HEX_WIDTH;
	int zY = m_nMyZoneIndex / WORLD_HEX_WIDTH;

	CHexCoord hc = CHexCoord::FromOffset( zX, zY );

	COMPILE_TIME_ASSERT( V_ARRAYSIZE( m_nZoneNeighbors ) == EHexEdge_Count );
	COMPILE_TIME_ASSERT( NEIGHBOR_HEX_COUNT == EHexEdge_Count );
	for ( int i = 0; i < V_ARRAYSIZE( m_nZoneNeighbors ); ++i )
	{
		CHexCoord hc_neighbor = hc + kHexDirection[i];
		int nX, nY;
		hc_neighbor.ToOffset( nX, nY );

		if ( nX < 0 || nX >= WORLD_HEX_WIDTH || nY < 0 || nY >= WORLD_HEX_HEIGHT )
		{
			m_nZoneNeighbors[i] = INVALID_DANGERZONE_INDEX;
		}
		else
		{
			m_nZoneNeighbors[i] = nY * WORLD_HEX_WIDTH + nX;
			Assert( m_nZoneNeighbors[i] >= 0 && m_nZoneNeighbors[i] < WORLD_HEX_NUM );
		}
	}
}

void CDangerZone::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

void CDangerZone::SetBombWave( int iWaveID )
{
	m_iWave = iWaveID;
}

void CDangerZone::SetDropOrder( int nDropOrder )
{
	m_nDropOrder = nDropOrder;
}

void CDangerZone::SetBombLaunchTime( float flBombLaunchTime, const Vector& vecLandingPos )
{
	m_flBombLaunchTime.Set( flBombLaunchTime );
	m_vecDangerZoneOriginStartedAt.Set( vecLandingPos );

	SetContextThink( &CDangerZone::SpawnDangerZoneCanister, flBombLaunchTime, "Danger:SpawnCanister" );
}

bool CDangerZone::BCanBePotentialEndGameHex()
{
	if ( m_eZoneHexType != k_EZoneHexType_Unknown )
	{
		return ( m_eZoneHexType == k_EZoneHexType_HasItems );
	}

	return ( GetPotentialEndGamePoints( NULL ) > 0 ) || ( m_eZoneHexType == k_EZoneHexType_HasItems );
}

int	CDangerZone::GetPotentialEndGamePoints( CUtlVector< CBaseEntity* > *pVecSpawnPoints /*= NULL*/ )
{
	if ( pVecSpawnPoints )
	{
		pVecSpawnPoints->RemoveAll();
	}

	int nTotalPoints = 0;
	const float flDistAwayFromNeighborSqr = Square( sv_dz_zone_hex_radius.GetFloat() + dev_dz_final_zone_radius.GetFloat() );
	const float flRadiusSqr = Square( sv_dz_zone_hex_radius.GetFloat() );
	const Vector2D vec2DOrigin = GetAbsOrigin().AsVector2D();
	FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), iPoint )
	{
		CBaseEntity *pEnt = IPointDZWeaponSpawn::AutoList()[iPoint]->GetEntity();
		Vector2D vecEntPos2D = pEnt->GetAbsOrigin().AsVector2D();

		// filter everything that's too far from this hex
		if ( vec2DOrigin.DistToSqr( vecEntPos2D ) > flRadiusSqr )
		{
			continue;
		}

		// make sure the point is far enough from each neighbor to not have final circle overlap with max expansion of neighbor
		bool bIntersectWithNeighbor = false;
		for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
		{
			CDangerZone *pNeighbor = GetNeighbor( iNeighbor );
			if ( pNeighbor && pNeighbor->GetAbsOrigin().AsVector2D().DistToSqr( vecEntPos2D ) < flDistAwayFromNeighborSqr )
			{
				bIntersectWithNeighbor = true;
				break;
			}
		}

		if ( !bIntersectWithNeighbor )
		{
			if ( pVecSpawnPoints )
			{
				pVecSpawnPoints->AddToTail( pEnt );
			}
			nTotalPoints++;
		}
	}

	// Cache the decision we made about this hex
	m_eZoneHexType = ( nTotalPoints > 0 ) ? k_EZoneHexType_HasItems : k_EZoneHexType_Empty;
	return nTotalPoints;
}


CDangerZone *CDangerZone::GetNeighbor( int nNeighborIdx )
{
	int nZoneIdx = m_nZoneNeighbors[ nNeighborIdx ];
	if ( nZoneIdx == -1 )
		return NULL;

	return GetDangerZoneController()->GetDangerZone( nZoneIdx );
}

CDangerZone *CDangerZone::GetOppositeNeighbor( int nNeighborIdx )
{
	int nOppositeNeighborIdx = ( nNeighborIdx + 3 ) % NEIGHBOR_HEX_COUNT;
	return GetNeighbor( nOppositeNeighborIdx );
}

DEVELOPMENT_ONLY_CONVAR( dev_dz_zone_extra_radius_delay, 0.5 );
void CDangerZone::UpdateExtraRadius()
{
	float flMaxNeighborZoneStartTime = 0.f;
	for ( int iNeighbor=0; iNeighbor<NEIGHBOR_HEX_COUNT; ++iNeighbor )
	{
		CDangerZone *pNeighbor = GetNeighbor( iNeighbor );

		// don't update final zone neighbor
		// they get update somewhere else
		if ( GetDangerZoneController()->IsFinalZone( pNeighbor ) )
		{
			return;
		}
			
		if ( pNeighbor && pNeighbor->GetZoneStartTime() > flMaxNeighborZoneStartTime )
		{
			flMaxNeighborZoneStartTime = pNeighbor->GetZoneStartTime();
		}
	}

	if ( flMaxNeighborZoneStartTime > 0.f )
	{
		float flTotalLerpTime = GetTotalLerpTime( sv_dz_zone_hex_radius.GetFloat() );
		float flDelay = dev_dz_zone_extra_radius_delay.GetFloat() * flTotalLerpTime;
		SetExtraRadius( dev_dz_zone_extra_radius_scale.GetFloat() * sv_dz_zone_hex_radius.GetFloat(), flMaxNeighborZoneStartTime + flDelay, flTotalLerpTime - flDelay );
	}
}

void CDangerZone::SetExtraRadius( float flExtraRadius, float flStartTime, float flTotalLerpTime )
{
	m_flExtraRadius = flExtraRadius;
	m_flExtraRadiusStartTime = flStartTime;
	m_flExtraRadiusTotalLerpTime = flTotalLerpTime;
}

void CDangerZone::Log()
{
	float flNormalLerpTime = GetTotalLerpTime( sv_dz_zone_hex_radius.GetFloat() );

	auto lambdaLogDangerZone = [&]( float flStartTime, const Vector& vecStartPos, float flStartRadius, float flEndTime, const Vector& vecEndPos, float flEndRadius )
	{
		float flRoundStart = GetDangerZoneController()->GetStartTime();
		UTIL_LogPrintf( "DANGERZONE[%d] start: time[ %f ] pos[ %f %f %f ] radius[ %f ] stop: time[ %f ] pos[ %f %f %f ] radius[ %f ]\n",
						m_nMyZoneIndex,
						flStartTime - flRoundStart, XYZ( vecStartPos ), flStartRadius,
						flEndTime - flRoundStart, XYZ( vecEndPos ), flEndRadius );
	};

	// log normal
	lambdaLogDangerZone( GetZoneStartTime(), GetBombLandingPos(), 0.f,
						GetZoneStartTime() + flNormalLerpTime, GetAbsOrigin(), sv_dz_zone_hex_radius.GetFloat() );

	// log extra radius
	lambdaLogDangerZone( m_flExtraRadiusStartTime.Get(), GetAbsOrigin(), sv_dz_zone_hex_radius.GetFloat(),
						m_flExtraRadiusStartTime.Get() + m_flExtraRadiusTotalLerpTime.Get(), GetAbsOrigin(), m_flExtraRadius.Get() );
}

CON_COMMAND( survival_check_num_possible_final_zone, "print out a number of all possible final zone" )
{
	if ( !GetDangerZoneController() || !GetDangerZoneController()->IsMasterDangerZoneEnabled() )
	{
		Warning( "Danger Zone not enabled. Make sure to be in the actual game to run this command." );
		return;
	}

	int nPossibleFinalZone = 0;
	for ( int i=0; i<GetDangerZoneController()->GetDangerZoneCount(); ++i )
	{
		CDangerZone *pZone = GetDangerZoneController()->GetDangerZone( i );
		if ( pZone && pZone->GetPotentialEndGamePoints() > 0 )
		{
			nPossibleFinalZone++;
		}
	}

	DevMsg( "[SURVIVAL] This map has [%d] possible final zones", nPossibleFinalZone );
}

#endif // !CLIENT_DLL

