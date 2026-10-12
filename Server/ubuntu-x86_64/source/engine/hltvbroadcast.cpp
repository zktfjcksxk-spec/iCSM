//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Broadcasting HLTV demo in 3-second chunks, preparing for SteamCast
// TODO test cases: start broadcast before relay server is ready; quit the server while broadcast hasn't flushed yet, make sure broadcast is properly stopped, flush is done and sends returned at least one confirm or error before quitting.
//
//=============================================================================//


#include <tier1/strtools.h>
#include <eiface.h>
#include <bitbuf.h>
#include <time.h>
#include "hltvbroadcast.h"
#include "hltvserver.h"
#include "demo.h"
#include "host_cmd.h"
#include "demofile/demoformat.h"
#include "filesystem_engine.h"
#include "net.h"
#include "networkstringtable.h"
#include "dt_common_eng.h"
#include "host.h"
#include "server.h"
#include "sv_steamauth.h"
#ifndef DEDICATED
#include "cl_steamauth.h"
#endif
#include "networkstringtableclient.h"
#include "tier1/fmtstr.h"
#include "engine_gcmessages.pb.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



static ISteamHTTP *s_pSteamHTTP = NULL;

ConVar tv_broadcast_keyframe_interval( "tv_broadcast_keyframe_interval", "3", FCVAR_RELEASE, "The frequency, in seconds, of sending keyframes and delta fragments to the broadcast relay server" );
ConVar tv_broadcast_keyframe_interval1( "tv_broadcast_keyframe_interval1", "3", FCVAR_RELEASE, "The frequency, in seconds, of sending keyframes and delta fragments to the broadcast1 relay server" );
ConVar tv_broadcast_startup_resend_interval( "tv_broadcast_startup_resend_interval", "10", FCVAR_RELEASE, "The interval, in seconds, of re-sending startup data to the broadcast relay server (useful in case relay crashes, restarts or startup data http request fails)" );
ConVar tv_broadcast_max_requests( "tv_broadcast_max_requests", "20", FCVAR_RELEASE, "Max number of broadcast http requests in flight. If there is a network issue, the requests may start piling up, degrading server performance. If more than the specified number of requests are in flight, the new requests are dropped." );
ConVar tv_broadcast_max_requests1( "tv_broadcast_max_requests1", "20", FCVAR_RELEASE, "Max number of broadcast1 http requests in flight. If there is a network issue, the requests may start piling up, degrading server performance. If more than the specified number of requests are in flight, the new requests are dropped." );
ConVar tv_broadcast_drop_fragments( "tv_broadcast_drop_fragments", "0", FCVAR_RELEASE | FCVAR_HIDDEN, "Drop every Nth fragment" );
ConVar tv_broadcast_terminate( "tv_broadcast_terminate", "1", FCVAR_RELEASE | FCVAR_HIDDEN, "Terminate every broadcast with a stop command" );
ConVar tv_broadcast_origin_auth( "tv_broadcast_origin_auth", "gocastauth" /*use something secure, like hMugYm7Lv4o5*/, FCVAR_RELEASE | FCVAR_HIDDEN, "X-Origin-Auth header of the broadcast POSTs" );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHLTVBroadcast::CHLTVBroadcast(CHLTVServer *pHltvServer) :m_pHltvServer( pHltvServer )
{
	m_flTimeout = 30;
	m_pFile = NULL;
	m_bIsRecording = false;
	m_nHttpRequestBacklogHighWatermark = 0;
	m_nMatchFragmentCounter = 0;
	m_flBroadcastKeyframeInterval = ( m_pHltvServer && m_pHltvServer->GetInstanceIndex() ) ? tv_broadcast_keyframe_interval1.GetFloat() : tv_broadcast_keyframe_interval.GetFloat();
	m_pCurrentBroadcastFragment = &m_FragmentQueue[ 0 ];
	m_pQueuedBroadcastFragment = &m_FragmentQueue[ 1 ];
}

CHLTVBroadcast::CMemoryStream::CMemoryStream() : m_Buffer( NET_MAX_PAYLOAD, NET_MAX_PAYLOAD )
{
	m_nCommitted = 0;
	m_pReserved = NULL;
}

CHLTVBroadcast::~CHLTVBroadcast()
{
	StopRecording();
}

void CHLTVBroadcast::OnMasterStarted()
{
	// we need some kind of unique id per match, when we can't have MatchID and use SteamID. This is it - it doesn't change if you restart broadcast during a match, but it changes every time a master proxy starts, which signifies a (re)start of tv server, which will always happen at the beginning of a match.
	m_nMasterCookie = Plat_GetTime(); 
	m_nMatchFragmentCounter = 0; // new match, start from the new match fragment counter
}

void CHLTVBroadcast::StartRecording( const char *pBroadcastUrl )
{
	StopRecording();	// stop if we're already recording
#ifndef DEDICATED
	s_pSteamHTTP = Steam3Client().SteamHTTP();
#endif

	if ( !s_pSteamHTTP )
		s_pSteamHTTP = Steam3Server().SteamHTTP();

	ConMsg( "Recording Broadcast\n" );

	m_flBroadcastKeyframeInterval = ( m_pHltvServer && m_pHltvServer->GetInstanceIndex() ) ? tv_broadcast_keyframe_interval1.GetFloat() : tv_broadcast_keyframe_interval.GetFloat();
	m_nLastWrittenTick = 0;
	m_nFrameCount = 0;
	m_nKeyframeTick = 0; // when broadcasting starts, we immediately send a keyframe

	//Tag it as -1 so we can really set the start tick when the first frame is written
	m_nStartTick = -1;
	m_nCurrentTick = 0;
	m_nSignonDataAckTick = -1;
	m_nSignonDataFragment = -1;

	m_bIsRecording = true;

	m_nDeltaTick = -1;

	m_mpKeyframe.Reset();
	m_mpLowLevelSend.Reset();
	m_nMaxKeyframeTicks = 0;
	m_nMaxLowLevelSendTicks = 0;
	m_nDecayMaxKeyframeTicks = 0;
	m_nKeyframeBytes = m_nDeltaFrameBytes = 0;
	m_nFailedHttpRequests = 0;
	m_mpFrame.Reset();

	// extern ConVar sv_mmqueue_reservation;
	if ( uint64 nMatchId = sv.GetMatchId() )
	{
		int nHltvInstance = m_pHltvServer->GetInstanceIndex();
		m_Url.Format( "%s/%llui%d", pBroadcastUrl, nMatchId, nHltvInstance );
	}
	else
	{
		uint64 nSteamId = Steam3Server().GetGSSteamID().ConvertToUint64();
		m_Url.Format( "%s/s%llut%llu", pBroadcastUrl, nSteamId, m_nMasterCookie );
	}
}

bool CHLTVBroadcast::IsRecording()
{
	return m_bIsRecording;
}

void CHLTVBroadcast::StopRecording( )
{
	if ( !m_bIsRecording )
		return;

	DevMsg( "Stop Broadcast @frag %d\n", m_nMatchFragmentCounter );

	if ( tv_broadcast_terminate.GetBool() )
	{
		CAutoSnip autoSnip( m_pCurrentBroadcastFragment, m_nCurrentTick, true ); // add the frame and snip if necessary
		autoSnip.GetStream()->WriteCmdHeader( dem_stop, GetRecordingTick(), 0 ); // just one cmd
	}
	QueueBroadcastDeltaFragment( "&final" );
	FlushBroadcastQueue();
	m_nMatchFragmentCounter += 2; // we need to create a hole in the broadcast in case we change our mind and start broadcast again some moments (or hours) later. This will force all clients to re-sync to the new keyframe

	// free up some memory
	m_ScratchBuffer.Purge(); 
	m_pQueuedBroadcastFragment->Purge();
	m_pCurrentBroadcastFragment->Purge();
	m_SignonDataStream.Reset();
	m_SignonDataStream.Purge();
	m_nStartTick = -1;
	m_nSignonDataAckTick = -1;

	for ( int i = 0; i < m_HttpRequests.Count(); ++i )
		m_HttpRequests[ i ]->DetachFromParent(); // TODO: if we're quitting, make sure that all the http requests finished before destroying the broadcast object
	m_HttpRequests.Purge(); // the elements will delete themselves

	// Note: some data fragments will still be in flight until they finish. We may start and stop another recording, and that's fine, SteamHTTP will continue uploading those data chunks until done or failed.
	
	m_bIsRecording = false;
}




void CHLTVBroadcast::WriteServerInfo( CMemoryStream &stream )
{
	stream.WriteCmdHeader( dem_signon, GetRecordingTick(), 0 );
	CInStreamMsgWithSize msg( stream, "CHLTVBroadcast::WriteServerInfo" );

	// on the master demos are using sv object, on relays hltv
	CBaseServer *pServer = m_pHltvServer->IsMasterProxy()?(CBaseServer*)(&sv):(CBaseServer*)(m_pHltvServer);

	CSVCMsg_ServerInfo_t serverinfo;
	m_pHltvServer->FillServerInfo( serverinfo ); // fill rest of info message
	serverinfo.WriteToBuffer( msg );

	// send first tick
	CNETMsg_Tick_t signonTick( m_nSignonTick, 0, 0, 0 );
	signonTick.WriteToBuffer( msg );

	// Write replicated ConVars to non-listen server clients only
	CNETMsg_SetConVar_t convars;
	
	// build a list of all replicated convars
	Host_BuildConVarUpdateMessage( convars.mutable_convars(), FCVAR_REPLICATED, true );
	if ( m_pHltvServer->IsMasterProxy() )
	{
		// for SourceTV server demos write set "tv_transmitall 1" even
		// if it's off for the real broadcast
		convars.AddToTail( "tv_transmitall", "1" );
		m_pHltvServer->FixupConvars( convars );
	}

	// write convars to demo
	convars.WriteToBuffer( msg );

	// write stringtable baselines
#ifndef SHARED_NET_STRING_TABLES
	if ( m_pHltvServer->m_StringTables )
		m_pHltvServer->m_StringTables->WriteBaselines( pServer->GetMapName(), msg, m_nSignonTick );
#endif

	// send signon state
	CNETMsg_SignonState_t signonMsg( SIGNONSTATE_NEW, pServer->GetSpawnCount() );
	signonMsg.WriteToBuffer( msg );
}


void CHLTVBroadcast::RecordCommand( const char *cmdstring )
{
	if ( !IsRecording() )
		return;

	if ( !cmdstring || !cmdstring[0] )
		return;

	CAutoSnip autoSnip( m_pCurrentBroadcastFragment, m_nCurrentTick, true ); // add the frame and snip if necessary
	autoSnip.GetStream()->WriteCmdHeader( dem_consolecmd, GetRecordingTick(), 0 );
	CInStreamMsg msg( *autoSnip.GetStream(), "RecordCommand", 1024 );
	CSVCMsg_Broadcast_Command_t cmd;
	cmd.set_cmd( cmdstring );
	cmd.WriteToBuffer( msg );
}


void CHLTVBroadcast::RecordServerClasses( CMemoryStream &stream, ServerClass *pClasses )
{
	stream.WriteCmdHeader( dem_datatables, GetRecordingTick(), 0 );
	CInStreamMsg buf( stream, "CHLTVBroadcast::RecordServerClasses", DEMO_RECORD_BUFFER_SIZE );

	// Send SendTable info.
	DataTable_WriteSendTablesBuffer( pClasses, &buf );

	// Send class descriptions.
	DataTable_WriteClassInfosBuffer( pClasses, &buf );

	if ( buf.GetNumBitsLeft() <= 0 )
	{
		Sys_Error( "unable to record server classes\n" );
	}

	// this was a dem_datatables in the demo file
}

// OBSOLETE
void CHLTVBroadcast::RecordStringTables( CMemoryStream &stream )
{
	stream.WriteCmdHeader( dem_stringtables, GetRecordingTick(), 0 );
	CInStreamMsg buf( stream, "CHLTVBroadcast::RecordStringTables", DEMO_RECORD_BUFFER_SIZE );
	m_pHltvServer->m_NetworkStringTables.WriteStringTables( buf , m_nSignonTick );
}



void CHLTVBroadcast::WriteSignonData()
{
	// on the master demos are using sv object, on relays hltv
	CBaseServer *pServer = m_pHltvServer->IsMasterProxy()?(CBaseServer*)(&sv):(CBaseServer*)(m_pHltvServer);

	m_nSignonTick = pServer->m_nTickCount;		

	m_SignonDataStream.Purge();
	m_nSignonDataFragment = m_nMatchFragmentCounter;

	WriteServerInfo( m_SignonDataStream );

	RecordServerClasses( m_SignonDataStream, serverGameDLL->GetAllServerClasses() );
	// RecordStringTables( m_SignonDataStream ); // <- this is redundand! The baseline already contains the stringtables for the right tick!

	{
		m_SignonDataStream.WriteCmdHeader( dem_signon, GetRecordingTick(), 0 );
		CInStreamMsgWithSize msg( m_SignonDataStream, "CHLTVBroadcast::WriteSignonData" );

		// use your class infos, CRC is correct
		// use your class infos, CRC is correct
		CSVCMsg_ClassInfo_t classmsg;
		classmsg.set_create_on_client( true );
		classmsg.WriteToBuffer( msg );

		// Write the regular signon now
		msg.WriteBits( m_pHltvServer->m_Signon.GetData(), m_pHltvServer->m_Signon.GetNumBitsWritten() );

		// write new state
		CNETMsg_SignonState_t signonMsg1( SIGNONSTATE_PRESPAWN, pServer->GetSpawnCount() );
		signonMsg1.WriteToBuffer( msg );
	}

	{
		// Dump all accumulated avatar data messages into signon portion of the demo file
		FOR_EACH_MAP_FAST( m_pHltvServer->m_mapPlayerAvatarData, iData )
		{
			m_SignonDataStream.WriteCmdHeader( dem_signon, GetRecordingTick(), 0 );
			CInStreamMsgWithSize msg( m_SignonDataStream, "CHLTVBroadcast::WriteSignonData(avatar data)" );
			CNETMsg_PlayerAvatarData_t &msgPlayerAvatarData = *m_pHltvServer->m_mapPlayerAvatarData.Element( iData );
			msgPlayerAvatarData.WriteToBuffer( msg );
		}

		// For official tournament servers also dump all the avatars of possible players into signon portion
		extern ConVar sv_mmqueue_reservation;
		extern ConVar sv_reliableavatardata;
		if ( ( sv_reliableavatardata.GetInt() == 2 ) && ( sv_mmqueue_reservation.GetString()[ 0 ] == 'Q' ) )
		{
			// CSteamID steamIdGs( SteamGameServer() ? SteamGameServer()->GetSteamID() : CSteamID() );
			CSteamID steamIdGs( Steam3Server().GetGSSteamID() );
			if ( !steamIdGs.IsValid() ) steamIdGs.SetEUniverse( k_EUniversePublic );
			CSteamID steamIdUser( 1, steamIdGs.GetEUniverse(), k_EAccountTypeIndividual );
			CUtlVector< AccountID_t > arrAvatarsToAddFromDisk;
			for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
				( pszNext = strchr( pszPrev, '[' ) ) != NULL; ( pszPrev = pszNext + 1 ) )
			{
				uint32 uiAccountId = 0;
				sscanf( pszNext, "[%x]", &uiAccountId );
				if ( uiAccountId &&	// valid account and not yet sent in previous loop
					( m_pHltvServer->m_mapPlayerAvatarData.Find( uiAccountId ) == m_pHltvServer->m_mapPlayerAvatarData.InvalidIndex() ) &&
					( arrAvatarsToAddFromDisk.Find( uiAccountId ) == arrAvatarsToAddFromDisk.InvalidIndex() ) )
				{
					arrAvatarsToAddFromDisk.AddToTail( uiAccountId );
				}
			}
			for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
				( pszNext = strchr( pszPrev, '{' ) ) != NULL; ( pszPrev = pszNext + 1 ) )
			{
				uint32 uiAccountId = 0;
				sscanf( pszNext, "{%x}", &uiAccountId );
				if ( uiAccountId &&	// valid account and not yet sent in previous loop
					( m_pHltvServer->m_mapPlayerAvatarData.Find( uiAccountId ) == m_pHltvServer->m_mapPlayerAvatarData.InvalidIndex() ) &&
					( arrAvatarsToAddFromDisk.Find( uiAccountId ) == arrAvatarsToAddFromDisk.InvalidIndex() ) )
				{
					arrAvatarsToAddFromDisk.AddToTail( uiAccountId );
				}
			}
			FOR_EACH_VEC( arrAvatarsToAddFromDisk, i )
			{
				steamIdUser.SetAccountID( arrAvatarsToAddFromDisk[i] );
				//
				// Try to load the avatar data for this player
				//
				CUtlBuffer bufAvatarData;
				CUtlBuffer bufAvatarDataDefault;
				CUtlBuffer *pbufUseRgb = NULL;
				if ( !pbufUseRgb &&
					g_pFullFileSystem->ReadFile( CFmtStr( "avatars/%llu.rgb", steamIdUser.ConvertToUint64() ), "MOD", bufAvatarData ) &&
					( bufAvatarData.TellPut() == 64 * 64 * 3 ) )
					pbufUseRgb = &bufAvatarData;
				if ( !pbufUseRgb &&
					g_pFullFileSystem->ReadFile( "avatars/default.rgb", "MOD", bufAvatarDataDefault ) &&
					( bufAvatarDataDefault.TellPut() == 64 * 64 * 3 ) )
					pbufUseRgb = &bufAvatarDataDefault;

				if ( pbufUseRgb )
				{
					CNETMsg_PlayerAvatarData_t msgPlayerAvatarData;
					msgPlayerAvatarData.set_rgb( pbufUseRgb->Base(), pbufUseRgb->TellPut() );
					msgPlayerAvatarData.set_accountid( steamIdUser.GetAccountID() );

					m_SignonDataStream.WriteCmdHeader( dem_signon, GetRecordingTick(), 0 );
					CInStreamMsgWithSize msg( m_SignonDataStream, "CHLTVBroadcast::WriteSignonData(avatar data)" );
					msgPlayerAvatarData.WriteToBuffer( msg );
				}
			}
		}
	}

	{
		m_SignonDataStream.WriteCmdHeader( dem_signon, GetRecordingTick(), 0 );
		CInStreamMsgWithSize msg( m_SignonDataStream, "CHLTVBroadcast::WriteSignonData(tail)" );
		// set view entity
		CSVCMsg_SetView_t viewent;
		viewent.set_entity_index( m_pHltvServer->m_nViewEntity );
		viewent.WriteToBuffer( msg );

		// Spawned into server, not fully active, though
		CNETMsg_SignonState_t signonMsg2( SIGNONSTATE_SPAWN, pServer->GetSpawnCount() );
		signonMsg2.WriteToBuffer( msg );
	}
}


void CHLTVBroadcast::SendSignonData()
{
	Assert( !m_SignonDataStream.IsEmpty() );
	Send( CFmtStr( "/%d/start?tick=%d&tps=%.1f&map=%s&protocol=%d", m_nSignonDataFragment, m_nStartTick, 1.0f / sv.GetTickInterval(), sv.GetMapName(), DEMO_PROTOCOL ), m_SignonDataStream );
	m_nSignonDataAckTick = m_nCurrentTick;
}

void CHLTVBroadcast::OnHttpRequestFailed()
{
	m_nFailedHttpRequests++;
}

void CHLTVBroadcast::OnHttpRequestResetContent()
{
	// may need to re-send the startup data - the http response says that the server hasn't received startup data yet at the time it was responding to the request
	if ( ( m_nCurrentTick - m_nSignonDataAckTick ) * sv.GetTickInterval() > tv_broadcast_startup_resend_interval.GetFloat() )
	{
		Msg( "Broadcast[%d] Re-sending signon data\n", m_pHltvServer->GetInstanceIndex() );
		// we haven't seen an OK response (meaning the startup tick is recognized by the server) in . Try to re-send
		SendSignonData();
	}
}

void CHLTVBroadcast::OnHttpRequestSuccess()
{
	m_nSignonDataAckTick = m_nCurrentTick;
}

void CHLTVBroadcast::Register( CHttpCallback *pCallback )
{
	m_HttpRequests.AddToTail( pCallback );
	m_nHttpRequestBacklogHighWatermark = Max( m_HttpRequests.Count(), m_nHttpRequestBacklogHighWatermark );
}

void CHLTVBroadcast::Unregister( CHttpCallback *pCallback )
{
	m_HttpRequests.FindAndFastRemove( pCallback );
}


void CHLTVBroadcast::FixupOperatorTick( int nMsgTick )
{
	// in most cases, fixup should be applied to the fragment currently being built very shortly after the frame was committed
	//if ( m_pCurrentBroadcastFragment->FixupOperatorTick( nMsgTick ) )
	{
	//	return; // in this case, there's nothing to do, we're still building the fragment
	}

	//if ( m_pQueuedBroadcastFragment->FixupOperatorTick( nMsgTick ) )
	{
		// in other cases, the just-queued fragment is awaiting a few more fixups
		//if ( m_pQueuedBroadcastFragment->m_nFixupTicks == m_pQueuedBroadcastFragment->m_Frames.Count() )
		if( !m_pQueuedBroadcastFragment->m_Frames.IsEmpty() && m_pQueuedBroadcastFragment->m_Frames.Tail().m_nTick <= nMsgTick )
		{
			FlushBroadcastQueue(); // we just fixed up all the ticks in this fragment - flush it to the broadcast
		}
		//return;
	}

	//Msg( "Fixup tick %d came too late. Current fragment %s, queued %s\n", nMsgTick, m_pCurrentBroadcastFragment->GetDebugDesc().Get(), m_pQueuedBroadcastFragment->GetDebugDesc().Get() );
}


CON_COMMAND( tv_broadcast_resend, "resend broadcast data to broadcast relay" )
{
	for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
		hltv->m_Broadcast.ResendStartup();
}


void CHLTVBroadcast::ResendStartup()
{
	if ( m_nStartTick != -1 )
	{
		// the signon data has been sent
		SendSignonData();
	}
}

void CHLTVBroadcast::WriteFrame( CHLTVFrame *pFrame, bf_write *additionaldata )
{
	Assert( m_pHltvServer->IsMasterProxy() ); // this works only on the master since we use sv.

	if ( !m_bIsRecording || !pFrame || pFrame->tick_count <= m_nCurrentTick )
		return; // guard against writing the same frames, or frames out of order

	m_nCurrentTick = pFrame->tick_count;
	bool bKeyFrame = ( m_nCurrentTick - m_nKeyframeTick ) * sv.GetTickInterval() >= m_flBroadcastKeyframeInterval; // is it time to cut off and broadcast the fragment of ticks recorded so far?

	if ( m_nStartTick == -1 )
	{
		m_nStartTick = pFrame->tick_count;
		WriteSignonData();
		SendSignonData();
		bKeyFrame = true;
	}
	else
	{
		CAutoSnip autoSnip( m_pCurrentBroadcastFragment, m_nCurrentTick, true ); // add the frame and snip if necessary

		autoSnip.GetStream()->WriteCmdHeader( dem_packet, GetRecordingTick(), 0 );
		CInStreamMsgWithSize msg( *autoSnip.GetStream(), "CHLTVBroadcast:Frame(delta)", NET_MAX_PAYLOAD );
		MICRO_PROFILE( m_mpFrame );
		RecordSnapshot( pFrame, additionaldata, msg, m_nDeltaTick );
		// update delta tick just like fake clients do
		m_nDeltaTick = pFrame->tick_count;
		m_nDeltaFrameBytes += msg.GetNumBytesWritten();
	}
	m_nLastWrittenTick = pFrame->tick_count;
	m_nFrameCount++;

	if ( bKeyFrame )
	{
		int numRequestsMaxInFlight = ( m_pHltvServer && m_pHltvServer->GetInstanceIndex() ) ? tv_broadcast_max_requests1.GetInt() : tv_broadcast_max_requests.GetInt();
		if ( m_HttpRequests.Count() > numRequestsMaxInFlight )
		{
			int nFragment = ++m_nMatchFragmentCounter;
			Warning( "Broadcast backlog of http requests in flight is too high (%d > %d), dropping %d/full and %d/delta.\n",
					 m_HttpRequests.Count(), numRequestsMaxInFlight, m_nMatchFragmentCounter, nFragment );
			m_pCurrentBroadcastFragment->Clear(); // drop the data
		}
		else
		{
			// upload the delta frames (IMPORTANT: including the current frame that we'll re-record the keyframe for) and the TOC (the last entry of TOC becomes useful only after we upload the current delta frames)
			// So, each 3-second fragment will full and delta frames has the full frame of the tick preceding the first delta frame tick
			QueueBroadcastDeltaFragment();
			// it's been 3 seconds already, let's re-send the keyframe - starting a new fragment with a full frame update
			BroadcastKeyframeFragment( pFrame, additionaldata );
			Assert( m_pCurrentBroadcastFragment->m_Stream.GetCommitSize() == 0 );
		}
	}
}


void CHLTVBroadcast::DumpStats()
{
	uint32 numKeyframes = m_nMatchFragmentCounter;
	int64 nKeyframeBytes = m_nKeyframeBytes, nDeltaFrameBytes = m_nDeltaFrameBytes;
	if ( numKeyframes > 1 )
	{
		// I'm missing the last keyframe, its delta portion is truncated
		nKeyframeBytes /= numKeyframes ;
		nDeltaFrameBytes /= numKeyframes ;
	}
	if ( numKeyframes )
	{
		Msg( "signup frag %d (%u bytes), frag counter %d.\nDelta frames: %s Bps, %.3f ms/frame.\nAvg Keyframe: %s Bytes, %.3f ms (%.1f max, %.1f recent max)\nHttp: %d failed, %d/%d max in flight\n",
			 m_nSignonDataFragment, m_SignonDataStream.GetCommitSize(),
			 m_nMatchFragmentCounter,
			 V_pretifynum( int( nDeltaFrameBytes / m_flBroadcastKeyframeInterval ) ),
			 m_mpFrame.GetAverageMilliseconds(),
			 V_pretifynum( nKeyframeBytes ),
			 m_mpKeyframe.GetAverageMilliseconds(),
			 CMicroProfiler::TimeBaseTicksToMilliseconds( m_nMaxKeyframeTicks ),
			 CMicroProfiler::TimeBaseTicksToMilliseconds( m_nDecayMaxKeyframeTicks ),
			 m_nFailedHttpRequests, m_HttpRequests.Count(), m_nHttpRequestBacklogHighWatermark
		);
		Msg( "http Send %.3f ms ave, %.3f ms max\n", m_mpLowLevelSend.GetAverageMilliseconds(), CMicroProfiler::TimeBaseTicksToMilliseconds( m_nMaxLowLevelSendTicks ) );
	}
	else
	{
		Msg( "no keyframes written\n" );
	}
}

void CHLTVBroadcast::RecordSnapshot( CHLTVFrame * pFrame, bf_write * additionaldata, bf_write &msg, int nDeltaTick )
{
	//first write reliable data
	bf_write *data = &pFrame->m_Messages[ HLTV_BUFFER_RELIABLE ];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	//now send snapshot data

	if ( host_frameendtime_computationduration > .1f )
		DevMsg( "CHLTVBroadcast::RecordSnapshot tick %.1f ms\n", host_frameendtime_computationduration * 1000 );

	// send tick time
	CNETMsg_Tick_t tickmsg( pFrame->tick_count, host_frameendtime_computationduration, host_frametime_stddeviation, host_framestarttime_stddeviation );
	tickmsg.WriteToBuffer( msg );


#ifndef SHARED_NET_STRING_TABLES
	// Update shared client/server string tables. Must be done before sending entities
	m_pHltvServer->m_StringTables->WriteUpdateMessageAtTick( NULL, pFrame->tick_count, MAX( m_nSignonTick, nDeltaTick ), msg );
#endif

	// get delta frame
	CClientFrame *deltaFrame = m_pHltvServer->GetClientFrame( nDeltaTick ); // NULL if delta_tick is not found or -1

	// send entity update, delta compressed if deltaFrame != NULL
	CSVCMsg_PacketEntities_t packetmsg;
	sv.WriteDeltaEntities( m_pHltvServer->m_MasterClient, pFrame, deltaFrame, packetmsg );
	packetmsg.WriteToBuffer( msg );

	// send all unreliable temp ents between last and current frame
	CSVCMsg_TempEntities_t tempentsmsg;
	CFrameSnapshot * fromSnapshot = deltaFrame ? deltaFrame->GetSnapshot() : NULL;
	sv.WriteTempEntities( m_pHltvServer->m_MasterClient, pFrame->GetSnapshot(), fromSnapshot, tempentsmsg, 255 );
	if ( tempentsmsg.num_entries() )
	{
		tempentsmsg.WriteToBuffer( msg );
	}

	// write sound data
	data = &pFrame->m_Messages[ HLTV_BUFFER_SOUNDS ];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	// write voice data
	data = &pFrame->m_Messages[ HLTV_BUFFER_VOICE ];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	// last write unreliable data
	data = &pFrame->m_Messages[ HLTV_BUFFER_UNRELIABLE ];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	if ( additionaldata && additionaldata->GetNumBitsWritten() )
	{
		msg.WriteBits( additionaldata->GetBasePointer(), additionaldata->GetNumBitsWritten() );
	}
}

void * CHLTVBroadcast::CMemoryStream::Reserve( uint nReserveBytes )
{
	Assert( !m_pReserved );
	m_Buffer.EnsureCapacity( m_nCommitted + nReserveBytes );
	m_pReserved = m_Buffer.Base() + m_nCommitted;
	return m_pReserved;
}

void CHLTVBroadcast::CMemoryStream::Append( const void *pData, uint nSize )
{
	V_memcpy( Reserve( nSize ), pData, nSize );
	Commit( nSize );
	
}

void CHLTVBroadcast::CMemoryStream::Commit( uint nCommitBytes )
{
	Assert( m_pReserved && m_nCommitted + nCommitBytes <= uint( m_Buffer.NumAllocated() ) );
	m_nCommitted += nCommitBytes;
	m_pReserved = NULL;
}


void CHLTVBroadcast::CMemoryStream::Purge()
{
	Assert( !m_nCommitted && !m_pReserved ); // we should not have anything waiting to be committed
	m_Buffer.Purge();
	m_nCommitted = 0;
	m_pReserved = NULL;
}

void CHLTVBroadcast::CMemoryStream::WriteCmdHeader( unsigned char cmd, int tick, int nPlayerSlot )
{
	unsigned char *pCmdHeader = ( unsigned char* ) Reserve( 6 );
	pCmdHeader[ 0 ] = cmd;
	*( int* )( pCmdHeader + 1 ) = tick;
	pCmdHeader[ 5 ] = nPlayerSlot;
	Commit( 6 );
}


void CHLTVBroadcast::CMemoryStream::Align( uint nAlign )
{
	Assert( !( nAlign & ( nAlign - 1 ) ) ); // alignment must be power of 2
	Assert( !m_pReserved ); // we can't align in the midst of committing an existing reservation
	if ( uint nGap = ( nAlign - m_nCommitted ) & ( nAlign - 1 ) )
	{
		void *pGap = Reserve( nGap );
		V_memset( pGap, 0, nGap );
		Commit( nGap );
	}
}

void CHLTVBroadcast::FlushBroadcastQueue()
{
	// send the collected delta stream payload
	// reconstitute the fragment in order
	m_pQueuedBroadcastFragment->BuildFragment( &m_ScratchBuffer );
	if ( !m_ScratchBuffer.IsEmpty() )
	{
		if ( m_pQueuedBroadcastFragment->m_nFixupTicks > 0 && m_pQueuedBroadcastFragment->m_nFixupTicks < m_pQueuedBroadcastFragment->m_Frames.Count() )
		{
			Msg( "Is fixup operator client running at low FPS? Only %d/%d fixup ticks available for %s\n", m_pQueuedBroadcastFragment->m_nFixupTicks, m_pQueuedBroadcastFragment->m_Frames.Count(), m_pQueuedBroadcastFragment->m_Url );
		}
		Send( m_pQueuedBroadcastFragment->m_Url, m_ScratchBuffer ); // Send previous batch of delta frames
	}
	m_ScratchBuffer.Reset();
	m_pQueuedBroadcastFragment->Clear();
}


// 
// Place the fragment (normally a chunk of 3 seconds) into the queue to be sent to origin server
// The queue is a delay introduced to wait for fixup operator data
//
void CHLTVBroadcast::QueueBroadcastDeltaFragment( const char *pExtraParams )
{
	FlushBroadcastQueue();
	
	V_snprintf( m_pCurrentBroadcastFragment->m_Url, sizeof( m_pCurrentBroadcastFragment->m_Url ), "/%d/delta?endtick=%d%s", m_nMatchFragmentCounter, m_nCurrentTick, pExtraParams );
	V_swap( m_pQueuedBroadcastFragment, m_pCurrentBroadcastFragment );

	extern bool IsCameramanOverrideActive();
	if ( !IsCameramanOverrideActive() )
	{
		FlushBroadcastQueue(); // flush all pending fragments immediately
	}
}


void CHLTVBroadcast::BroadcastKeyframeFragment( CHLTVFrame *pKeyFrame, bf_write *pKeyFrameAdditionalData )
{
	m_nKeyframeTick = m_nCurrentTick;
	int nFragment = ++m_nMatchFragmentCounter;
	Assert( m_ScratchBuffer.IsEmpty() );
	{
		m_ScratchBuffer.WriteCmdHeader( dem_packet, GetRecordingTick(), 0 );
		CInStreamMsgWithSize msg( m_ScratchBuffer, "CHLTVBroadcast:FullFrame", NET_MAX_PAYLOAD );
		CMicroProfilerSample sample;
		RecordSnapshot( pKeyFrame, pKeyFrameAdditionalData, msg, -1 );
		int64 nElapsedTicks = sample.GetElapsed();
		m_mpKeyframe.Add( nElapsedTicks );
		m_nMaxKeyframeTicks = Max( m_nMaxKeyframeTicks, nElapsedTicks );
		m_nDecayMaxKeyframeTicks = Max( m_nDecayMaxKeyframeTicks * 933 / 1000, nElapsedTicks ); // 0.933 ^ 20 = .25 , this will decay 1/4 every minute
	}
	m_nKeyframeBytes += m_ScratchBuffer.GetCommitSize();
	if ( CHttpCallback *pCallback = Send( CFmtStr( "/%d/full?tick=%d", nFragment, m_nCurrentTick ), m_ScratchBuffer ) ) // send the keyframe for the following batch of delta frames
	{
		CEngineGotvSyncPacket *pMsg = new CEngineGotvSyncPacket;
		pMsg->set_match_id( sv.GetMatchId() );
		pMsg->set_instance_id( m_pHltvServer->GetInstanceIndex() );
		pMsg->set_signupfragment( m_nSignonDataFragment );
		pMsg->set_currentfragment( nFragment );
		pMsg->set_tickrate( 1.0f / sv.GetTickInterval() );
		pMsg->set_tick( m_nCurrentTick );
		pMsg->set_keyframe_interval( m_flBroadcastKeyframeInterval );
		pCallback->SetProtobufMsgForGCUponSuccess( pMsg );
	}
	m_ScratchBuffer.Reset();
}



// protocol is very simple: a=<account/match id> & t=  <type, i=initial/startup, k=keyframe/full frame, d=delta frames> & 
CHLTVBroadcast::CHttpCallback * CHLTVBroadcast::Send( const char* pPath, CMemoryStream &stream )
{
	return Send( pPath, stream.Base(), stream.GetCommitSize() );
}


CHLTVBroadcast::CHttpCallback * CHLTVBroadcast::Send( const char* pPath, const void *pBase, uint nSize )
{
	if ( !s_pSteamHTTP )
	{
		Warning( "HLTV Broadcast cannot send data because steam http is not available. Are you logged into Steam?\n" );
		return NULL;
	}
	if ( tv_broadcast_drop_fragments.GetInt() > 0 && ( rand() % tv_broadcast_drop_fragments.GetInt() ) == 0 )
	{
		Msg( "Dropping %d bytes to %s%s\n", nSize, GetUrl(), pPath );
		return NULL;
	}

	return LowLevelSend( m_Url + pPath, pBase, nSize );
}


CHLTVBroadcast::CHttpCallback * CHLTVBroadcast::LowLevelSend( const CUtlString &path, const void *pBase, uint nSize )
{
	CMicroProfilerGuard mpg( &m_mpLowLevelSend );

	HTTPRequestHandle hRequest = s_pSteamHTTP->CreateHTTPRequest( k_EHTTPMethodPOST, path.Get() );
	if ( !hRequest )
	{
		Warning( "Cannot create http put: %s, %u bytes lost\n", path.Get(), nSize );
		return NULL;
	}
	s_pSteamHTTP->SetHTTPRequestNetworkActivityTimeout( hRequest, m_flTimeout );
	const char *pOriginAuth = tv_broadcast_origin_auth.GetString();
	if ( pOriginAuth && *pOriginAuth )
	{
		if ( !s_pSteamHTTP->SetHTTPRequestHeaderValue( hRequest, "X-Origin-Auth", pOriginAuth ) )
		{
			Warning( "Cannot set http X-Origin-Auth\n" );
		}
	}
	if ( !s_pSteamHTTP->SetHTTPRequestRawPostBody( hRequest, "application/octet-stream", ( uint8* )pBase, nSize ) )
	{
		Warning( "Cannot set http post body for %s, %u bytes\n", path.Get(), nSize );
		s_pSteamHTTP->ReleaseHTTPRequest( hRequest );
		return NULL;
	}

	SteamAPICall_t hCall;
	CHttpCallback *pResult = NULL;
	if ( s_pSteamHTTP->SendHTTPRequest( hRequest, &hCall ) && hCall )
	{
		pResult = new CHttpCallback( this, hRequest, path.Get() );
		SteamAPI_RegisterCallResult( pResult, hCall );
	}
	else
	{
		s_pSteamHTTP->ReleaseHTTPRequest( hRequest );
	}

	m_nMaxLowLevelSendTicks = Max( m_nMaxLowLevelSendTicks, mpg.GetElapsed() );

	return pResult;
}


CHLTVBroadcast::CHttpCallback::CHttpCallback( CHLTVBroadcast *pParent, HTTPRequestHandle hRequest, const char *pResource ) :
	m_pParent( pParent ), m_hRequest( hRequest ), m_Resource( pResource ), m_pProtobufMsgForGCUponSuccess( NULL )
{
	m_iCallback = HTTPRequestCompleted_t::k_iCallback;
	pParent->Register( this );
}

CHLTVBroadcast::CHttpCallback::~CHttpCallback()
{
	s_pSteamHTTP->ReleaseHTTPRequest( m_hRequest );
	if ( m_pParent )
		m_pParent->Unregister( this );

	delete m_pProtobufMsgForGCUponSuccess;
}

void CHLTVBroadcast::CHttpCallback::Run( void *pvParam )
{	// success!
	Run( pvParam, false, 0 );
}

void CHLTVBroadcast::CHttpCallback::Run( void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall )
{
	EHTTPStatusCode nStatus = ( ( HTTPRequestCompleted_t * )pvParam )->m_eStatusCode;

	if ( bIOFailure )
	{
		Msg( "Broadcast[%d] IO Failure, Http code %d on %s\n", m_pParent ? m_pParent->m_pHltvServer->GetInstanceIndex() : -1, int( nStatus ), m_Resource.Get() );
		if ( m_pParent )
			m_pParent->OnHttpRequestFailed();
	}
	else
	{
		if ( nStatus == k_EHTTPStatusCode205ResetContent )
		{
			if ( m_pParent )
				m_pParent->OnHttpRequestResetContent();
		}
		else
		{
			if ( nStatus == k_EHTTPStatusCode200OK ) // we should always get a 200
			{
				if ( m_pProtobufMsgForGCUponSuccess )
				{
					serverGameDLL->EngineGotvSyncPacket( m_pProtobufMsgForGCUponSuccess );
				}
			}
			else
			{
				Msg( "Broadcast[%d] Relay returned Http code %d on %s\n", m_pParent ? m_pParent->m_pHltvServer->GetInstanceIndex() : -1, int( nStatus ), m_Resource.Get() );
			}
		}
	}
	delete this;
}

void CHLTVBroadcast::CHttpCallback::SetProtobufMsgForGCUponSuccess( const CEngineGotvSyncPacket *pProtobufMsgForGCUponSuccess )
{
	Assert( !m_pProtobufMsgForGCUponSuccess && pProtobufMsgForGCUponSuccess );
	if ( pProtobufMsgForGCUponSuccess != m_pProtobufMsgForGCUponSuccess )
	{
		delete m_pProtobufMsgForGCUponSuccess;
		m_pProtobufMsgForGCUponSuccess = pProtobufMsgForGCUponSuccess;
	}
}

void CHLTVBroadcast::CQueuedFragment::Purge()
{
	Clear();
	m_Frames.Purge();
	m_Stream.Purge();
}

void CHLTVBroadcast::CQueuedFragment::Clear()
{
	m_Url[ 0 ] = '\0';
	m_Frames.RemoveAll();
	m_nFixupTicks = 0;
	m_Stream.Reset();
}

CHLTVBroadcast::Frame_t & CHLTVBroadcast::CQueuedFragment::MarkFrame(int nTick)
{
	// we don't call this method with nTick < current tick, but we might in the future
	// in that case we'll need to implement a search here and remove the search for tick from other places
	if ( !m_Frames.IsEmpty() )
	{
		Assert( nTick >= m_Frames.Tail().m_nTick );
		if ( nTick == m_Frames.Tail().m_nTick )
		{
			return m_Frames.Tail();
		}
	}
	Frame_t &frame = *m_Frames.AddToTailGetPtr();
	frame.m_nTick = nTick;
	frame.m_nSnipList = INVALID_OFFSET;
	return frame;
}

void CHLTVBroadcast::CQueuedFragment::BuildFragment( CMemoryStream *pOut )
{
	Assert( pOut->GetCommitSize() == 0 );
	uint nStreamSize = m_Stream.GetCommitSize();
	if ( nStreamSize < sizeof( Snip_t ) )
		return;

	for ( const Frame_t &frame : m_Frames )
	{
		for ( uint nSnipOffset = frame.m_nSnipList; nSnipOffset < nStreamSize - sizeof( Snip_t ); )
		{
			const Snip_t *pSnip = ( const Snip_t* )( ( ( const byte* )m_Stream.Base() ) + nSnipOffset );
			if ( pSnip->nSize <= nStreamSize - nSnipOffset - sizeof( Snip_t ) )
			{
				pOut->Append( pSnip + 1, pSnip->nSize );
			}
			else
			{
				Warning( "Frame %d has corrupted snip @%u(%u,%u) in %u byte stream\n", frame.m_nTick, nSnipOffset, pSnip->nNextSnip, pSnip->nSize, nStreamSize );
				break; // we can actually continue building the fragment
			}
			nSnipOffset = pSnip->nNextSnip;
		}
	}
}

bool CHLTVBroadcast::CQueuedFragment::FixupOperatorTick( const CCLCMsg_HltvFixupOperatorTick& msgFixup )
{
	if ( m_Frames.IsEmpty() )
		return false;

	int nFixupTick = msgFixup.tick();

	if ( nFixupTick < m_Frames[ 0 ].m_nTick || nFixupTick > m_Frames.Tail().m_nTick )
		return false;

	for ( int f = m_Frames.Count(); f-- > 0; )
	{
		if ( m_Frames[ f ].m_nTick != nFixupTick )
			continue;

		CAutoSnip autoSnip( &m_Stream, &m_Frames[ f ].m_nSnipList ); // we're prepending this little snip, so passing the pointer to the very first offset

		m_Stream.WriteCmdHeader( dem_packet, nFixupTick, 0 );// write 6 bytes demo file header
		CInStreamMsgWithSize msg( m_Stream, "FixupOperatorTick", NET_MAX_PAYLOAD ); // write 4 bytes chunk size, followed by network packet payload (compatible with demo file format)
					
		CCLCMsg_HltvFixupOperatorTick_t fwdMsg;
		fwdMsg.set_tick( nFixupTick );
		SetMsgVector( fwdMsg.mutable_origin(), AsVector( msgFixup.origin() ) );
		SetMsgQAngle( fwdMsg.mutable_eye_angles(), AsQAngle( msgFixup.eye_angles() ) );
		fwdMsg.WriteToBuffer( msg );

		m_nFixupTicks++;

		// here ~CAutoSnip() will write the size of the snip into the (possibly reallocated) stream
		return true;
	}
	return false;
}

CUtlString CHLTVBroadcast::CQueuedFragment::GetDebugDesc()
{
	if ( m_Frames.IsEmpty() )
		return "no frames";
	CUtlString out;
	out.Format( "frames %d-%d; %d fixup ticks; %u bytes", m_Frames[ 0 ].m_nTick, m_Frames.Tail().m_nTick, m_nFixupTicks, m_Stream.GetCommitSize() );
	return out;
}

// Create a Snip_t header and insert it into the linked list of snips
// it's possible to optimize it a bit by continuing the previous snip, but it'd make code even less readable
void CHLTVBroadcast::CAutoSnip::Construct( CMemoryStream *pStream, uint *pNextSnip )
{
	m_pStream = pStream;
	pStream->Align( 8 );
	m_nSnipOffset = m_pStream->GetCommitSize();
	Snip_t *pNewSnip = ( Snip_t *)m_pStream->Reserve( sizeof( Snip_t ) );
	pNewSnip->nNextSnip = *pNextSnip;
	*pNextSnip = m_nSnipOffset;
	pNewSnip->nSize = 0;
	pStream->Commit( sizeof( Snip_t ) );
}

//////////////////////////////////////////////////////////////////////////
// Start and end a snip in the given fragment, allocate the frame if necessary
// 
CHLTVBroadcast::CAutoSnip::CAutoSnip( CQueuedFragment *pFragment, int nFrame, bool bAddToTail )
{
	Frame_t &frame = pFragment->MarkFrame( nFrame );
	uint32 *pSnipList = &frame.m_nSnipList;
	if ( bAddToTail )
	{
		uint nCommitSize = pFragment->m_Stream.GetCommitSize();
		while ( *pSnipList < nCommitSize )
		{
			Snip_t *pNextSnip = ( Snip_t* )( pFragment->m_Stream.Base() + *pSnipList );
			pSnipList = &pNextSnip->nNextSnip;
		}
		Assert( *pSnipList == INVALID_OFFSET );
	}
	Construct( &pFragment->m_Stream, pSnipList );
}

CHLTVBroadcast::CAutoSnip::~CAutoSnip()
{
	Snip_t *pNewSnip = ( Snip_t* )( m_pStream->Base() + m_nSnipOffset );
	pNewSnip->nSize = m_pStream->GetCommitSize() - m_nSnipOffset - sizeof( Snip_t );
}
