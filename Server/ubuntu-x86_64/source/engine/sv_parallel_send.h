//===== Copyright (c) Valve Corporation, All rights reserved. ======//
#pragma once

#include "vstdlib/jobthread.h"
#include "tier0/threadtools.h"

class CGameClient;
class CFrameSnapshot;

//#pragma optimize("",off)
#ifdef _DEBUG
#define VALIDATION_LEVEL 1
#else
#define VALIDATION_LEVEL 0
#endif


// align by cache line size to assist interlocked operations
class CParallelSendTask
{
public:
	const int m_nClientCount;
	CGameClient** m_pClients;
	CFrameSnapshot *m_pSnapshot;
	int m_nHltvMaxAckCount;
	
	uint32 m_nNextPackClient;

	enum PhaseEnum_t {
		PHASE_COMPUTE_PACKS,
		PHASE_SEND_SNAPSHOTS,
		PHASE_TEST
	};
	PhaseEnum_t m_nPhase;

	uint32 m_nNextSendClient;
	uint32 m_nPreparePVSInfoCounter;

	struct PackState_t
	{
		uint32 nPackInProgress;
		uint32 nNeedsPacking;
		uint32 nPackCompleted;
	};
	PackState_t m_EntityPackState[ MAX_EDICTS / 32 ];
	CUtlVectorFixedGrowable< uint32, 32 > m_ClientPackingStage;

	int m_nJobCount;
	//byte m_Padding[ 60 ]; - if there's temporal aliasing between the fill block and chokepoint counters, we should put them in different cache lines
	//CInterlockedUInt m_nChokepoint1Countdown;

#if VALIDATION_LEVEL
	int64 m_nKickTime;
	CInterlockedInt m_nJobStart;
	struct JobStartRec_t
	{
		int m_nThreadId;
		int64 m_nTime;
	};
	JobStartRec_t m_JobStartRec[ 64 ];
#endif
public:
	CParallelSendTask( int nJobCount, int clientCount, CGameClient** clients, CFrameSnapshot *snapshot, int nHltvMaxAckCount );
	void Run();

	void WaitWorkerJobs(  );

	void KickWorkerJobs( PhaseEnum_t nPhase );

	void OnFinishedPackEntities();
	void Test();
	static void Shutdown();

	void RunWorkerThread();

	void ComputePacks();
	void ComputePacks_Client( uint nClient );

	void SetupFrameForPacking_Client( uint nClient );

	void SendSnapshots();
	void SendSnapshot_Client( uint nSnapshot );

	bool CompletePackingAndSendSnapshot_One();
	void CompletePackingAndSendSnapshot_Client( uint nClient );

	void ScheduleEntitiesForPacking( CGameClient *pClient );
	int PackScheduledEntities( );
	void PrepareEntities_SingleThreaded();

	bool AllPacksCompleted_Client( uint nClient );
	void ValidateAllPacksCompleted_Client( uint nClient );
	uint GetEBlockCount()const;
	 
	void ValidatePostRunState();
protected:
	class CWorkerJob: public CJob
	{
	protected:
		CParallelSendTask *m_pTask;
		
		virtual int AddRef() OVERRIDE;
		virtual int Release() OVERRIDE;
	public:
		CWorkerJob() : m_pTask( NULL )
		{}
		void Destroy() { delete this; }
		void Init( CParallelSendTask *pTask );
		virtual JobStatus_t DoExecute() OVERRIDE;
		//virtual JobStatus_t DoAbort( bool bDiscard )
	};

	friend class CDebugState;

	static CUtlVectorFixedGrowable< CWorkerJob*, 32 > s_WorkerJobs;
};

enum { SENDTASK_PROFILER_COUNT = 4 };

