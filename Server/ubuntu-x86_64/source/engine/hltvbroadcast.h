//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVBROADCAST_HDR
#define HLTVBROADCAST_HDR
#ifdef _WIN32
#pragma once
#endif

#include <filesystem.h>
//#include "demo.h"
#include "broadcast.h"
#include "tier0/microprofiler.h"
#include "tier1/utlincrementalvector.h"
#include "tier1/utlhashtable.h"
#include "steam/steam_api.h"
#include "steam/isteamhttp.h"

class CHLTVFrame;
class CHLTVServer;

class CEngineGotvSyncPacket;	// forward declare protobuf message here
class CCLCMsg_HltvFixupOperatorTick;


class CHLTVBroadcast
{
protected:
	class CMemoryStream
	{
	protected:
		CUtlMemory< uint8 > m_Buffer; // data chunk that will get uploaded
		uint m_nCommitted; // the number of bytes in payload buffer that will need to be uploaded
		uint8 *m_pReserved;
		// the total number of bytes allocated in payload buffer includes committed portion + reserved portion that is being filled in right now. m_pPayloadReserved == NULL after upload, otherwise we forgot to commit.

	public:
		CMemoryStream();
		void *Reserve( uint nReserveBytes );
		void Append( const void*pData, uint nSize );
		const byte *GetReservedBase()const { return ( byte*) ( m_Buffer.Base() + m_nCommitted ); }
		const byte *Base()const { return ( byte* )m_Buffer.Base(); }
		uint GetReservedSize()const { return m_Buffer.Count() - m_nCommitted; }
		void Commit( uint nCommitBytes );
		void Purge();
		bool IsEmpty()const { return m_nCommitted == 0; }
		uint GetCommitSize()const { return m_nCommitted; }
		void Reset() { m_nCommitted = 0; }
		void WriteCmdHeader( unsigned char cmd, int tick, int nPlayerSlot );
		void Align( uint nAlign );
	};

	class CInStreamMsg : public bf_write
	{
	protected:
		CMemoryStream &m_Stream;
		// int m_nCmd;
		// int m_nTick;
		// int m_nPlayerSlot,
	public:
		CInStreamMsg( CMemoryStream &stream, const char *pDebugName, uint nReserveSize = 256 * 1024 ) :
			m_Stream( stream ),
			bf_write( pDebugName, stream.Reserve(nReserveSize), nReserveSize )
		{

		}
		~CInStreamMsg()
		{
			m_Stream.Commit( GetNumBytesWritten() );
		}
	};

	class CInStreamMsgWithSize : public CInStreamMsg
	{
	protected:
	public:
		CInStreamMsgWithSize( CMemoryStream &stream, const char *pDebugName, uint nReserveSize = 256 * 1024 ) :
			CInStreamMsg( stream, pDebugName, nReserveSize )
		{
			this->WriteLong( 0 ); // we'll write this in the end
		}
		~CInStreamMsgWithSize()
		{
			// the length of the message, after the initial 4 bytes that contain the length of the message. Binary compatible with .dem file
			*( int32* )( this->GetBasePointer() ) = this->GetNumBytesWritten() - sizeof( int32 ); 
		}
	};

	class CHttpCallback : public CCallbackBase
	{
	public:
		CHttpCallback( CHLTVBroadcast *pParent, HTTPRequestHandle hRequest, const char *pResource );
		~CHttpCallback();
		virtual void Run( void *pvParam ) OVERRIDE; // success; HTTPRequestCompleted_t
		virtual void Run( void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall ) OVERRIDE; // result; HTTPRequestCompleted_t
		void DetachFromParent() { m_pParent = NULL; }

		void SetProtobufMsgForGCUponSuccess( const CEngineGotvSyncPacket *pProtobufMsgForGCUponSuccess );

	public:
		int m_nIncrementalVectorIndex;
	protected:
		virtual int GetCallbackSizeBytes() OVERRIDE { return sizeof( HTTPRequestCompleted_t ); }
		CHLTVBroadcast *m_pParent;
		HTTPRequestHandle m_hRequest;
		CUtlString m_Resource;
		const CEngineGotvSyncPacket *m_pProtobufMsgForGCUponSuccess;
	};

	enum ConstEnum_t { INVALID_OFFSET  = 0x7FFFFFFF };
	// contiguous block that will go into the final broadcast stream. Snips can be recorded out of order and then reconstituted
	struct Snip_t
	{
		uint nSize; // the size of the snip in bytes; just the size of data that follows this struct, not including sizeof(Snip_t)
		uint nNextSnip; // next snip in the stream; INVALID_OFFSET = end of list
	};

	// each fragment has ~380 frames (3 seconds worth at 128 Hz) in a tournament
	// each frame has a linked list of snips that may not be continuous
	struct Frame_t
	{
		int m_nTick;
		uint m_nSnipList; // the first snip offset in bytes from the m_Stream start; INVALID_OFFSET = empty list
	};

	class CQueuedFragment
	{
	public:
		char m_Url[ 128 ];
		CUtlVector < Frame_t > m_Frames;
		int m_nFixupTicks;	    // number of fixup ticks; when this number == m_Frames.Count , we're not receiving any more fixup ticks and can finally send the fragment to the relay server
		CMemoryStream m_Stream; // the stream of snips that get reconstituted later

		void Purge();
		void Clear();
		Frame_t &MarkFrame( int nTick );
		void BuildFragment( CMemoryStream *pOut );
		bool FixupOperatorTick( const CCLCMsg_HltvFixupOperatorTick& msg );
		CUtlString GetDebugDesc();
	};

	class CAutoSnip
	{
		uint m_nSnipOffset;
		CMemoryStream *m_pStream;
	protected:
		void Construct( CMemoryStream *pStream, uint *pNextSnip );
	public:
		CAutoSnip( CMemoryStream *pStream, uint *pNextSnip ) { Construct( pStream, pNextSnip ); }
		CAutoSnip( CQueuedFragment *pFragment, int nFrame, bool bAddToTail );
		~CAutoSnip();

		CMemoryStream *GetStream() { return m_pStream; }
	};

public:
	CHLTVBroadcast( CHLTVServer *pHltvServer );
	virtual ~CHLTVBroadcast();
	int GetRecordingTick( void )
	{
		return m_nLastWrittenTick/* - m_nStartTick*/;
	}

	void	OnMasterStarted();
	void	StartRecording( const char *pBroadcastUrl );
	const char *GetUrl()const { return m_Url.Get(); }
	void	SetSignonState( int state ) {}; // not need by HLTV recorder
	bool	IsRecording( void );
	void	PauseRecording( void ) {};
	void	ResumeRecording( void ) {};
	void	StopRecording();

	void DumpStats();

	void	RecordCommand( const char *cmdstring );
	void	RecordUserInput( int cmdnumber ) {};  // not need by HLTV recorder
	//void	RecordMessages( bf_read &data, int bits );
	void	RecordServerClasses( CMemoryStream &stream, ServerClass *pClasses );
	void	RecordStringTables( CMemoryStream &stream );

	void	ResetDemoInterpolation( void ) {};
public:
	void	WriteFrame( CHLTVFrame *pFrame, bf_write *additionaldata = NULL );
	void	ResendStartup();

	void	RecordSnapshot( CHLTVFrame * pFrame, bf_write * additionaldata, bf_write &msg, int nDeltaTick );

	void	CloseFile();
	void	Reset();

	void	WriteServerInfo( CMemoryStream &stream );
	void	WriteSignonData();  // write all necessary signon data and returns written bytes
	void	WriteMessages( CMemoryStream &stream, unsigned char cmd );

	void	SendSignonData();

	int		GetMaxAckTickCount();

	void	OnHttpRequestFailed();
	void	OnHttpRequestResetContent();
	void	OnHttpRequestSuccess();

	void Register( CHttpCallback *pCallback );
	void Unregister( CHttpCallback *pCallback );
	void	FixupOperatorTick( int nMsgTick );


	//void FixupOperatorTick( const CCLCMsg_HltvFixupOperatorTick& msg );
protected:
	void QueueBroadcastDeltaFragment( const char *pExtraParams = "");
	void BroadcastKeyframeFragment( CHLTVFrame *pKeyFrame, bf_write *pKeyFrameAdditionalData );
	void FlushBroadcastQueue();
	CHttpCallback * Send( const char* pPath, CMemoryStream &stream );
	CHttpCallback * Send( const char* pPath, const void *pBase, uint nSize );
	CHttpCallback * LowLevelSend( const CUtlString &path, const void *pBase, uint nSize );
protected:
	bool			m_bIsRecording;
	int				m_nFrameCount;
	int				m_nKeyframeTick; // last broadcast keyframe tick; also, the tick of the first delta frame in m_DeltaFrameStream
	int				m_nStartTick;
	int				m_nDeltaTick;
	int				m_nSignonTick;
	int				m_nCurrentTick;
	int				m_nSignonDataAckTick; // when signon data was sent out last
	int				m_nLastWrittenTick;
	uint64			m_nMasterCookie;
	CHLTVServer		*m_pHltvServer;

	CMicroProfiler	m_mpKeyframe, m_mpFrame, m_mpLowLevelSend;
	int64 m_nMaxKeyframeTicks, m_nDecayMaxKeyframeTicks, m_nMaxLowLevelSendTicks;
	int64 m_nKeyframeBytes, m_nDeltaFrameBytes; // statistics

	FileHandle_t m_pFile;

	// The following fields are exclusive to the HTTP broadcast implementation, they are not needed for writing into file, memory or netchan streams
	//HTTPRequestHandle m_hHTTPRequestHandle;
	CMemoryStream m_ScratchBuffer; // used for double-buffering memory to avoid excessive reallocations
	CQueuedFragment *m_pQueuedBroadcastFragment; // 1-element broadcast queue; 1 element is enough as it's usually 3 seconds and we only need to hold it long enough for fixup operator to come back with data after queuing (usually much less than 3 seconds )
	CQueuedFragment *m_pCurrentBroadcastFragment;
	CQueuedFragment m_FragmentQueue[2];
	CMemoryStream m_SignonDataStream;
	int m_nSignonDataFragment;
	CUtlString m_Url;
	float m_flTimeout;
	int	m_nFailedHttpRequests;

	friend class CHttpCallback;
	CUtlIncrementalVector< CHttpCallback > m_HttpRequests; // requests in flight
	int m_nHttpRequestBacklogHighWatermark;

	int m_nMatchFragmentCounter;
	float m_flBroadcastKeyframeInterval;

};




#endif // HLTVBROADCAST_HDR
