//===== Copyright (c) Valve Corporation, All rights reserved. ======//
#include "server_pch.h"
#include "sv_parallel_send.h"
#include "server.h"
#include "framesnapshot.h"
#include "networkstringtable.h"
#include "tier0/microprofiler.h"
#include "tier0/etwprof.h"
#include "tier0/spinlock.h"
#include "tier1/utlhashtable.h"

void SendTask_SetupFrame( CGameClient * pClient, CFrameSnapshot * snapshot );
void SV_PackEntity( int edictIdx, edict_t* edict, ServerClass* pServerClass, CFrameSnapshot *pSnapshot );
void InvalidateSharedEdictChangeInfos();
void SV_ParallelSendSnapshot( CGameClient *& pClient );
extern CNetworkStringTableContainer *networkStringTableContainerServer;
void SV_FillHLTVData( CFrameSnapshot *pSnapshot, edict_t *edict, int iValidEdict );
void SV_EnsureInstanceBaseline( ServerClass *pServerClass, int iEdict, SerializedEntityHandle_t handle );

CUtlVectorFixedGrowable< CParallelSendTask::CWorkerJob*, 32 > CParallelSendTask::s_WorkerJobs;

CMicroProfiler g_mpPackEntity[4];



class CDebugState
{
#if VALIDATION_LEVEL
	CParallelSendTask::PackState_t m_EntityPackState[ MAX_EDICTS / 32 ];
	uint32 m_nTransmit[ MAX_EDICTS / 32 ];
	CClientFrame *m_pFrame;
#endif
public:
	CDebugState( CParallelSendTask *pTask, uint nClient )
	{
#if VALIDATION_LEVEL
		uint nBlocks = MAX_EDICTS / 32;//pTask->GetEBlockCount();
		V_memcpy( m_EntityPackState, pTask->m_EntityPackState, nBlocks );
		m_pFrame = pTask->m_pClients[ nClient ]->m_pCurrentFrame;
		if( m_pFrame )
		{
			V_memcpy( m_nTransmit, m_pFrame->transmit_entity.Base(), nBlocks );
		}
#endif
	}
};

CParallelSendTask::CParallelSendTask( int nJobCount, int clientCount, CGameClient** clients, CFrameSnapshot *snapshot, int nHltvMaxAckCount ):
	m_nJobCount( nJobCount ),
	m_nClientCount( clientCount),
	m_pClients( clients),
	m_pSnapshot( snapshot ),
	m_nHltvMaxAckCount( nHltvMaxAckCount )
{
	m_ClientPackingStage.SetCount( m_nClientCount );
	m_ClientPackingStage.FillWithValue( 0 );
	// none of the netities are packed
	V_memset( m_EntityPackState, 0, GetEBlockCount() * sizeof( PackState_t ) );
	COMPILE_TIME_ASSERT( MAX_EDICTS < 010000 ); // edict incies should fit into 16 bits
	COMPILE_TIME_ASSERT( LOG2_BITS_PER_INT == 5 ); // processing edicts in batches of 32, corresponding to a block of bits in m_nEntityPacked bits

	// int64 nStart = GetTimebaseRegister(); -- cost: 30'000 ticks/ 7 jobs cold; 70-300 ticks hot.
	while ( s_WorkerJobs.Count() < nJobCount )
	{
		CWorkerJob *pJob = new CWorkerJob;
		s_WorkerJobs.AddToTail( pJob );
	}
	// int64 nTime = GetTimebaseRegister() - nStart; Msg( "CParallelSendTask: %llu ticks\n", nTime );
}






void CParallelSendTask::Run()
{
	//m_nChokepoint1Countdown = m_nClientCount;

	m_nNextPackClient = 0; // note: we could preallocate client 0 for packing, then we'd assign 1 here
	m_nNextSendClient = 0;
	
	m_nPreparePVSInfoCounter = 0; // 

	m_nJobCount = Min<int>( m_nJobCount, m_nClientCount - 1 );
	PrepareEntities_SingleThreaded();

	// to avoid any multithreading effects (test in single-threaded mode), uncomment:
	// m_nJobCount = 0;

	KickWorkerJobs( PHASE_COMPUTE_PACKS );
	
	// give the main thread something to do while waiting - incidentally, this reduces the job spin-up cost a bit (by a mutex wake-up time)
	// ComputePacks_Client; pack the preallocated client, if any
	RunWorkerThread(); // finish packing the rest of the clients

	// now we're really close to the end of all jobs
	WaitWorkerJobs();
	Assert( m_nNextPackClient == m_nClientCount );

	OnFinishedPackEntities(); // we could actually run this on a thread in one of the worker jobs, and it would be beneficial if we could save on synchronization costs
	// but the job system is pretty rigid and doesn't allow us to create job dependency graphs

	// The equivalent of: ParallelProcess( m_pClients, m_nClientCount, &SV_ParallelSendSnapshot );
	KickWorkerJobs( PHASE_SEND_SNAPSHOTS );

	SendSnapshots();

	WaitWorkerJobs( );

	ValidatePostRunState();
}


void CParallelSendTask::ValidatePostRunState()
{
#ifdef DBGFLAG_ASSERT
	Assert( m_nNextSendClient == m_nClientCount );
	for ( uint nBlockCount = GetEBlockCount(), nBlock = 0; nBlock < nBlockCount; ++nBlock )
	{
		Assert( m_EntityPackState[ nBlock ].nPackCompleted == m_EntityPackState[ nBlock ].nNeedsPacking );
		for ( int c = 0; c < m_nClientCount; ++c )
		{
			if ( m_pClients[ c ]->IsHltvReplay() )
				continue; // clients in HLTV replay use HLTV stream that has already been pre-packed for them by HLTV master client. No need to do any packing while streaming HLTV contents

			CClientFrame *frame = m_pClients[ c ]->m_pCurrentFrame;

			uint nMustTransmit = frame->transmit_entity.Base()[ nBlock ];
			Assert( ( nMustTransmit & m_EntityPackState[ nBlock ].nPackCompleted ) == nMustTransmit );
		}
	}
#endif
}


void CParallelSendTask::KickWorkerJobs( PhaseEnum_t nPhase )
{
	// overhead: ~10us Linux
#if VALIDATION_LEVEL
	m_nKickTime = GetTimebaseRegister();
	m_nJobStart = 0;
#endif
	m_nPhase = nPhase;
	for ( int i = 0; i < m_nJobCount; ++i )
	{
		//delete s_WorkerJobs[ i ];
		CWorkerJob *pJob = s_WorkerJobs[ i ];// = new CWorkerJob;
		pJob->Init( this );
		g_pThreadPool->AddJob( pJob );
	}
}


void CParallelSendTask::WaitWorkerJobs()
{
	if ( m_nJobCount )
		g_pThreadPool->YieldWait( ( CJob** ) s_WorkerJobs.Base(), m_nJobCount );

#if 0 // VALIDATION_LEVEL
	CUtlHashtable< int > threadIds;
	for ( int nJob = 0; nJob < m_nJobStart && nJob < ARRAYSIZE( m_JobStartRec ); ++nJob )
	{
		bool bThreadFirstSeen = false;
		threadIds.InsertIfNotFound( m_JobStartRec[ nJob ].m_nThreadId, &bThreadFirstSeen );
		if ( bThreadFirstSeen )
		{
			int64 nTime = m_JobStartRec[ nJob ].m_nTime;
			if ( nTime > m_nKickTime )
				g_mpPackEntity[ 3 ].Add( nTime - m_nKickTime );
		}
	}
#endif
}



void CParallelSendTask::OnFinishedPackEntities()
{
	InvalidateSharedEdictChangeInfos(); // can be parallelized

	// <sergiy> DirectUpdate is not necessary: I can remove mirror tables, and then it will be superfluous
#ifndef SHARED_NET_STRING_TABLES
	if ( m_nHltvMaxAckCount >= 0 )
	{// copy string updates from server to hltv stringtable
		networkStringTableContainerServer->DirectUpdate( m_nHltvMaxAckCount ); // !!!! WARNING: THIS IS NOT THREAD SAFE! MEMORY CORRUPTION GUARANTEED WITH MULTIPLE HLTV SERVERS!
	}
#endif
}



void CParallelSendTask::Shutdown()
{
	for ( int i = 0; i < s_WorkerJobs.Count(); ++i )
		s_WorkerJobs[ i ]->Destroy();
	s_WorkerJobs.Purge();
}


void CParallelSendTask::RunWorkerThread()
{
#if VALIDATION_LEVEL
	int nJobStart = m_nJobStart++;
	if ( nJobStart < ARRAYSIZE( m_JobStartRec ) )
	{
		// latency : 50us-100us/worker Linux
		m_JobStartRec[ nJobStart ].m_nThreadId = ThreadGetCurrentId();
		m_JobStartRec[ nJobStart ].m_nTime = GetTimebaseRegister();
	}
#endif
	CMicroProfilerGuardTS grd( &g_mpPackEntity[ 0 ] ); // parallel_send 12 overhead: 4x Windows, 2.2x Linux
	//CETWScope etwScope( "SendTaskWorker" );
	switch ( m_nPhase )
	{
	case PHASE_COMPUTE_PACKS:
		ComputePacks();
		break;
	case PHASE_TEST:
		Test();
		break;
	default:
		Assert( m_nPhase == PHASE_SEND_SNAPSHOTS );
		SendSnapshots();
		break;
	}
}


CInterlockedInt g_nWorkerjobRefs;

int CParallelSendTask::CWorkerJob::AddRef()
{
	g_nWorkerjobRefs++;
	return 1;
}

int CParallelSendTask::CWorkerJob::Release()
{
	g_nWorkerjobRefs--;
	return 1;
}

void CParallelSendTask::CWorkerJob::Init( CParallelSendTask *pTask )
{
	m_pTask = pTask;
	Reset();
}

JobStatus_t CParallelSendTask::CWorkerJob::DoExecute()
{
	m_pTask->RunWorkerThread();

	return JOB_OK;
}

void CParallelSendTask::ComputePacks()
{
	// the following step is not necessary for correctness; I'm speculatively precomputing PVS info so that there's no contention 
	// in the worker threads that do CheckTransmit on every client later. This is only for multithreaded scalability. 
	// without this precomputation, the whole thing doesn't scale well, and almost doesn't scale beyond 3-4 threads.
	serverGameEnts->PreparePVSInfo_Parallel( m_pSnapshot->m_pValidEntities, m_pSnapshot->m_nValidEntities, &m_nPreparePVSInfoCounter );

	InterlockedIterate( &m_nNextPackClient, m_nClientCount, this, &CParallelSendTask::ComputePacks_Client );

	// when we continue packing entities, some PVSs (those computed on other threads) may not have finished updating yet.
	// IsInPVS() may be called, which reads cluster pointer and count, so IsInPVS must block until PVS is recomputed.

	while ( PackScheduledEntities() > 0 )
	{
		// pack entities while there are entities to pack
		continue;
	}
}

void CParallelSendTask::SendSnapshots()
{
	InterlockedIterate( &m_nNextSendClient, m_nClientCount, this, &CParallelSendTask::SendSnapshot_Client );
}




void CParallelSendTask::ComputePacks_Client( uint nClient )
{
	SetupFrameForPacking_Client( nClient );
	PackScheduledEntities();
}


void CParallelSendTask::SetupFrameForPacking_Client( uint nClient )
{
	CFrameSnapshot *pSnapshot = m_pSnapshot;

	CGameClient *pClient = m_pClients[ nClient ];
	SendTask_SetupFrame( pClient, pSnapshot );

	// the following is a transposed parallel version of PackEntities_Normal
	// we don't need to pack entities for Hltv Replay, because those entities
	// have been packed long time ago. They just need to be sent now.
	if ( !pClient->IsHltvReplay() )
	{
		// now, m_pCurrentFrame is ready
		ScheduleEntitiesForPacking( pClient );
	}
	m_ClientPackingStage[ nClient ] = 1;
}

bool CParallelSendTask::CompletePackingAndSendSnapshot_One()
{
	bool bClientsNeedSending = false;
	for ( int i = 0; i < m_nClientCount; ++i )
	{
		if ( m_ClientPackingStage[ i ] >= 2 )
			continue;
		bClientsNeedSending = true;
		if ( ThreadInterlockedAssignIf( &m_ClientPackingStage[ i ], 2, 1 ) )
		{
			// stage 2: send snapshot for a client that has the frame fully set up
			CompletePackingAndSendSnapshot_Client( i );
		}
	}
	return bClientsNeedSending;
}

void CParallelSendTask::SendSnapshot_Client( uint nClient )
{
	ValidateAllPacksCompleted_Client( nClient );
	SV_ParallelSendSnapshot( m_pClients[ nClient ] );
}


void CParallelSendTask::CompletePackingAndSendSnapshot_Client( uint nClient )
{
	if ( m_pClients[ nClient ]->IsHltvReplay() )
	{
		// special case: we didn't schedule entities, we didn't pack them, we don't need to wait on them, we just need to send the previously-recorded snapshot
		SV_ParallelSendSnapshot( m_pClients[ nClient ] );
	}
	else
	{
		// wait for all the needed entity packs to complete
		while ( !AllPacksCompleted_Client( nClient ) )
		{
			if ( 0 == PackScheduledEntities() )
			{
				// there's nothing more to pack, just sleep
				ThreadSleep( 0 );
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Threading bug on frame #F:
		//  SvClient 1 finishes packing all its entities,
		//           sends a stringtable update
		//  while SvClient 2 hasn't finished yet. Then,
		//  SvClient 2 finds an entity #33 whose baseline isn't in the stringtable yet and adds it, and sends an update to Client 2 successfully
		//
		//  But SvClient 1 has already sent an update, which didn't have entity #33 baseline
		//  Client 1 receives, parses and acknowledges the update #F (having no idea about entity #33)
		//  SvClient 1 receives the ack and since entity #33 baseline is marked created/updated on frame #F, thinks that Client 1 knows about entity #33
		//  subsequently, stringtable updates are all shifted on Client 1

		ThreadMemoryBarrier(); // acquire
		SendSnapshot_Client( nClient );
	}
}



void CParallelSendTask::PrepareEntities_SingleThreaded()
{
	CFrameSnapshot *snapshot = m_pSnapshot;
	for ( int iValidEdict = 0; iValidEdict < snapshot->m_nValidEntities; ++iValidEdict )
	{
		int index = snapshot->m_pValidEntities[ iValidEdict ];

		Assert( index < snapshot->m_nNumEntities );

		edict_t* edict = &sv.edicts[ index ];

		// if HLTV is running save PVS info for each entity
		SV_FillHLTVData( snapshot, edict, iValidEdict );
	}
}


bool CParallelSendTask::AllPacksCompleted_Client( uint nClient )
{
	const uint32 *pTransmit = m_pClients[ nClient ]->m_pCurrentFrame->transmit_entity.Base();
	for ( uint nBlockCount = GetEBlockCount(), nBlock = 0; nBlock < nBlockCount; ++nBlock )
	{
		uint32 nNeedToTransmit = pTransmit[ nBlock ];
		if ( ( nNeedToTransmit & m_EntityPackState[ nBlock ].nPackCompleted ) != nNeedToTransmit )
		{
			return false;
		}
	}
	//ValidateAllPacksCompleted_Client( nClient );
	return true; // all entities checked out - they have completed packing
}

void CParallelSendTask::ValidateAllPacksCompleted_Client( uint nClient )
{
	if ( VALIDATION_LEVEL > 0 && !m_pClients[ nClient ]->IsHltvReplay() )
	{
		const uint32 *pTransmit = m_pClients[ nClient ]->m_pCurrentFrame->transmit_entity.Base();
		for ( uint nBlockCount = GetEBlockCount(), nBlock = 0; nBlock < nBlockCount; ++nBlock )
		{
			uint32 nNeedToTransmit = pTransmit[ nBlock ];
			uint32 nPackCompleted = m_EntityPackState[ nBlock ].nPackCompleted;
			uint32 nNotYetPacked = nNeedToTransmit & ~nPackCompleted;
			if ( nNotYetPacked )
			{
				DebuggerBreak();
			}
		}
	}
}

uint CParallelSendTask::GetEBlockCount() const
{
	return ( ( m_pSnapshot->m_nNumEntities + 31 ) / 32 );
}


void CParallelSendTask::Test()
{
	volatile int nCount = RandomInt( 0, 3000 );
	for ( volatile int i = 0; i < nCount; ++i )
		continue;
}



// Overhead : 1us Linux, 10us Windows
void CParallelSendTask::ScheduleEntitiesForPacking( CGameClient *pClient )
{
	Assert( !pClient->IsHltvReplay() ); // clients in HLTV replay use HLTV stream that has already been pre-packed for them by HLTV master client. No need to do any packing while streaming HLTV contents

	CClientFrame *frame = pClient->m_pCurrentFrame;
	for ( uint nBlockCount = GetEBlockCount(), nPackBlock = 0; nPackBlock < nBlockCount; ++nPackBlock )
	{
		uint32 nNewEntitiesToTransmit = frame->transmit_entity.Base()[ nPackBlock ]; // these entities need to be added to the list of entities needing packing
		ThreadInterlockedOr( &m_EntityPackState[ nPackBlock].nNeedsPacking, nNewEntitiesToTransmit );
	}
}



// Overhead without SV_PackEntity: 35us Linux (up to 12 threads, constant), 250us Windows
int CParallelSendTask::PackScheduledEntities( ) 
{
	int nEntitiesPacked = 0;
	CFrameSnapshot *pSnapshot = m_pSnapshot;
	// decide which entities haven't been packed yet and grab them to pack
	for ( uint nBlockCount = GetEBlockCount(), nPackBlock = 0; nPackBlock < nBlockCount; ++nPackBlock )
	{
		Assert( nPackBlock < ARRAYSIZE( m_EntityPackState ) );
		volatile PackState_t *pPackState = m_EntityPackState + nPackBlock;
		for(;; )
		{
			uint32 nAlreadyPacked = pPackState->nPackInProgress;
			uint32 nLeftToPack = pPackState->nNeedsPacking & ~nAlreadyPacked;
			if ( !nLeftToPack )
				break; // nothing left to pack
			uint32 nToPack = Plat_BitScanForward( nLeftToPack );
			if ( ThreadInterlockedAssignIf( &pPackState->nPackInProgress, nAlreadyPacked | ( 1 << nToPack ), nAlreadyPacked ) )
			{
				int index = nPackBlock * 32 + nToPack;
				edict_t* edict = &sv.edicts[ index ];
				{
					ServerClass *pServerClass = pSnapshot->m_pEntities[ index ].m_pClass;
					//CMicroProfilerSample unguard;
					SV_PackEntity( index, edict, pServerClass, pSnapshot );
					//ThreadInterlockedExchangeAdd64( ( int64* ) &g_mpPackEntity[3].m_numTimeBaseTicks, -unguard.GetElapsed() );
				}
				++nEntitiesPacked;
				AssertDbg( framesnapshotmanager->GetPackedEntity( *pSnapshot, index ) );
				ThreadMemoryBarrier(); // release
				ThreadInterlockedOr<uint32>( &pPackState->nPackCompleted, 1 << nToPack );// mark this entity as packed
			}
		} 
	}
	return nEntitiesPacked;
}

