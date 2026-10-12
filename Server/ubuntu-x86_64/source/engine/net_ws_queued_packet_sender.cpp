//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"

#include "tier0/vprof.h"
#include "tier1/utlvector.h"
#include "tier1/utlpriorityqueue.h"

#include <steamnetworkingsockets/isteamnetworkingsockets.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar net_queue_trace( "net_queue_trace", "0", FCVAR_ACCESSIBLE_FROM_THREADS );

class CQueuedPacketSender : public CThread, public IQueuedPacketSender
{
public:
	CQueuedPacketSender();
	~CQueuedPacketSender();

	// IQueuedPacketSender

	virtual bool Setup();
	virtual void Shutdown();
	virtual bool IsRunning() { return CThread::IsAlive(); }

	virtual void ClearQueuedPacketsForChannel( INetChannel *pChan ) OVERRIDE;
	virtual void QueuePacketToSteamNetConnection( INetChannel *pChan, HSteamNetConnection hConn, const void *buf, int len, uint32 msecDelay ) OVERRIDE;
	virtual void QueuePacketToNetAdr( INetChannel *pChan, const netadr_t &to, SOCKET s, const void *buf, int len, uint32 msecDelay ) OVERRIDE;
	virtual bool HasQueuedPackets( const INetChannel *pChan ) const OVERRIDE;
private:

	// CThread Overrides
	virtual bool Start( unsigned int nBytesStack = 0 );
	virtual int Run();

private:

	class CQueuedPacket
	{
	public:
		uint32				m_unSendTime;
		const void			*m_pChannel; // We just use this as an arbitrary identifier
		CUtlVector<char>	buf;

		HSteamNetConnection	m_hSteamNetConn;

		netadr_t			m_netadr;
		SOCKET				m_netadrSocket;

		// We want the list sorted in ascending order, so note that we return > rather than <
		static bool LessFunc( CQueuedPacket * const &lhs, CQueuedPacket * const &rhs )
		{
			return lhs->m_unSendTime > rhs->m_unSendTime;
		}
	};

	void QueuePacketInternal( CQueuedPacket *pPacket, INetChannel *pChan, const void *buf, int len, uint32 msecDelay );

	CUtlPriorityQueue< CQueuedPacket * > m_QueuedPackets;
	CThreadMutex m_QueuedPacketsCS;
	CThreadEvent m_hThreadEvent;
	volatile bool m_bThreadShouldExit;
};

static CQueuedPacketSender g_QueuedPacketSender;
IQueuedPacketSender *g_pQueuedPackedSender = &g_QueuedPacketSender;


CQueuedPacketSender::CQueuedPacketSender() :
	m_QueuedPackets( 0, 0, CQueuedPacket::LessFunc )
{
	SetName( "QueuedPacketSender" );
	m_bThreadShouldExit = false;
}

CQueuedPacketSender::~CQueuedPacketSender()
{
	Shutdown();
}

bool CQueuedPacketSender::Setup()
{
	return Start();
}

bool CQueuedPacketSender::Start( unsigned nBytesStack )
{
	Shutdown();

	if ( CThread::Start( nBytesStack ) )
	{
		// Ahhh the perfect cross-platformness of the threads library.
#ifdef IS_WINDOWS_PC
		SetPriority( THREAD_PRIORITY_HIGHEST );
		ThreadSetDebugName( GetThreadHandle(), "CQueuedPacketSender" );
#elif POSIX
		//SetPriority( PRIORITY_MAX );
#endif

		m_bThreadShouldExit = false;

		return true;
	}
	else
	{
		return false;
	}
}

void CQueuedPacketSender::Shutdown()
{
	if ( !IsAlive() )
		return;
		
#ifdef _WIN32
	if ( !GetThreadHandle() )
	{
		Msg( "-->Shutdown %p\n", GetThreadHandle() );
	}
#endif

	m_bThreadShouldExit = true;
	m_hThreadEvent.Set();
	
	Join(); // Wait for the thread to exit.

	while ( m_QueuedPackets.Count() > 0 )
	{
		delete m_QueuedPackets.ElementAtHead();
		m_QueuedPackets.RemoveAtHead();
	}
	m_QueuedPackets.Purge();
}

void CQueuedPacketSender::ClearQueuedPacketsForChannel( INetChannel *pChan )
{
	AUTO_LOCK( m_QueuedPacketsCS );

	for ( int i = m_QueuedPackets.Count()-1; i >= 0; i-- )
	{
		CQueuedPacket *p = m_QueuedPackets.Element( i );
		if ( p->m_pChannel == pChan )
		{
			m_QueuedPackets.RemoveAt( i );
			delete p;
		}
	}
}

bool CQueuedPacketSender::HasQueuedPackets( const INetChannel *pChan ) const
{
	AUTO_LOCK( m_QueuedPacketsCS );

	for ( int i = 0; i < m_QueuedPackets.Count(); ++i )
	{
		const CQueuedPacket *p = m_QueuedPackets.Element( i );
		if ( p->m_pChannel == pChan )
		{
			return true;
		}
	}

	return false;
}

void CQueuedPacketSender::QueuePacketToSteamNetConnection( INetChannel *pChan, HSteamNetConnection hConn, const void *buf, int len, uint32 msecDelay )
{
	CQueuedPacket *pPacket = new CQueuedPacket;
	Assert( hConn != k_HSteamNetConnection_Invalid );
	pPacket->m_hSteamNetConn = hConn;
	pPacket->m_netadrSocket = (SOCKET)-1;
	QueuePacketInternal( pPacket, pChan, buf, len, msecDelay );
}

void CQueuedPacketSender::QueuePacketToNetAdr( INetChannel *pChan, const netadr_t &to, SOCKET s, const void *buf, int len, uint32 msecDelay )
{
	CQueuedPacket *pPacket = new CQueuedPacket;
	pPacket->m_hSteamNetConn = k_HSteamNetConnection_Invalid;
	pPacket->m_netadrSocket = s;
	pPacket->m_netadr = to;
	QueuePacketInternal( pPacket, pChan, buf, len, msecDelay );
}

void CQueuedPacketSender::QueuePacketInternal( CQueuedPacket *pPacket, INetChannel *pChan, const void *buf, int len, uint32 msecDelay )
{
	int nMaxQueuedPackets = 1024;
	if ( m_QueuedPackets.Count() > nMaxQueuedPackets )
	{
		static int nWarnings = 5;
		if ( --nWarnings > 0 )
		{
			Warning( "CQueuedPacketSender: num queued packets >= nMaxQueuedPackets. Not queueing anymore.\n" );
		}
		delete pPacket;
		return;
	}

	// Finish setting up the packet
	uint32 msNow = Plat_MSTime();
	pPacket->m_pChannel = pChan;
	pPacket->m_unSendTime = msNow + msecDelay;
	pPacket->buf.CopyArray( (char*)buf, len );

	// Grab the lock at the last moment so we hold it for as short a time as possible
	AUTO_LOCK( m_QueuedPacketsCS );

	m_QueuedPackets.Insert( pPacket );

	// Tell the thread that we have a queued packet.
	// !SPEED! Do we really need to do this?  Shouldn't this only be necessary if the new packet we are queuing
	// is the new head of the queue?  Presumably the thread is already scheduled to wake up on time for the
	// current head
	m_hThreadEvent.Set();
}

int CQueuedPacketSender::Run()
{
	 // Normally TT_INFINITE but we wakeup every 500ms just in case.
	uint32 waitIntervalNoPackets = 500;
	uint32 waitInterval = waitIntervalNoPackets;
	while ( 1 )
	{
		m_hThreadEvent.Wait( waitInterval );
		{
			// Someone signaled the thread. Either we're being told to exit or 
			// we're being told that a packet was just queued.
			if ( m_bThreadShouldExit )
				return 0;
		}

		// Assume nothing to do and that we'll sleep again
		waitInterval = waitIntervalNoPackets;

		// OK, now send a packet.
		{
            SNPROF("NET_SendToImpl");
			AUTO_LOCK( m_QueuedPacketsCS );
		
			// We'll pull all packets we should have sent by now and send them out right away
			uint32 msNow = Plat_MSTime();

			bool bTrace = net_queue_trace.GetBool();

			while ( m_QueuedPackets.Count() > 0 )
			{
				CQueuedPacket *pPacket = m_QueuedPackets.ElementAtHead();
				if ( pPacket->m_unSendTime > msNow )
				{
					// Sleep until next we need this packet
					waitInterval = pPacket->m_unSendTime - msNow;
					if ( bTrace )
					{
						Warning( "SQ:  sleeping for %u msecs at %f\n", waitInterval, Plat_FloatTime() );
					}
					break;
				}

				if ( pPacket->m_hSteamNetConn != k_HSteamNetConnection_Invalid )
				{
					SteamNetworkingSockets()->SendMessageToConnection( pPacket->m_hSteamNetConn, pPacket->buf.Base(), pPacket->buf.Count(), k_ESteamNetworkingSendType_UnreliableNoDelay );
				}
				else if ( pPacket->m_netadr.GetType() == NA_NULL )
				{
					// It's a bot; don't do anything. Note: we DO want this code deep here because bots only
					// try to send packets when sv_stressbots is set, in which case we want it to act as closely
					// as a real player as possible.
				}
				else
				{

					if ( bTrace )
					{
						Warning( "SQ:  sending %d bytes at %f\n", pPacket->buf.Count(), Plat_FloatTime() );
					}

					NET_SendToNetAdrRaw
					( 
						pPacket->m_netadrSocket, 
						pPacket->buf.Base(), 
						pPacket->buf.Count(), 
						pPacket->m_netadr,
						-1 
					);
				}
				
				delete pPacket;
				m_QueuedPackets.RemoveAtHead();
			}
		}
	}
}

