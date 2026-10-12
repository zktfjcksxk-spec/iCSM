//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// TODO:
//  - USE A MEMPOOL OF SOME SORT
//  - GET STALE FRAME REMOVAL WORKING
//  - MAKE THREAD SAFE - NEED TO BE ABLE TO WRITE A .DEM FILE ON A SEP THREAD AND
//		NOT BE THROWING AWAY ELEMENTS THAT ARE BEING READ.  NEED A FLAG IN DATACHUNK
//		FOR WHETHER AN INSTANCE IS "IN USE," IE BEING READ TO WRITE TO A DEM FILE.
//
//===============================================================================

#include "demobuffer.h"
#include "edict.h"
#include "host.h"
#include "tier1/mempool.h"
#include "vstdlib/jobthread.h"
#include "demo.h"

#ifndef DEDICATED
#include "cdll_int.h"
#include "client.h"
#endif

#include "tier0/memdbgon.h"		// NOTE: Must go last!

#ifndef DEDICATED
#if !defined( _DEBUG )
// These are the buffers defining how demo data is flushed to disk:
// We allocate 5 MB buffer (worth about 5 min of gameplay)
// once over 4 MB is used up we will commit the first 1 MB to disk
// This throttles disk IO, and ensures that the last 3 MB are always
// contained in memory and not committed to disk to prevent file peeking
// for cheating purposes during gameplay
#define DISK_DEMO_BUFFER_TOTAL_SIZE (5*1024*1024)
#define DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE (4*1024*1024)
#define DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE (1*1024*1024)
#else
// Smaller sizes in debug engine.dll build to hammer on the subsystems involved
#define DISK_DEMO_BUFFER_TOTAL_SIZE (5*20*1024)
#define DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE (4*20*1024)
#define DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE (1*20*1024)
#endif
#endif

//-----------------------------------------------------------------------------
// Specialty class with overrides for stream buffer
//-----------------------------------------------------------------------------
class CDiskDemoBuffer : public IDemoBuffer
{
public:
	CDiskDemoBuffer()
	:	m_pBuffer( NULL )
	{
		m_nDecodedOffset = -1;
	}

	~CDiskDemoBuffer()
	{
		m_pBuffer->Close();
		delete m_pBuffer;
	}

	virtual bool Init( DemoBufferInitParams_t const& params )
	{
		// Convert to proper type
		StreamDemoBufferInitParams_t const* pParams = dynamic_cast< StreamDemoBufferInitParams_t const* >( &params );		Assert( pParams );

		// Allocate buffer
		m_pBuffer = new CUtlStreamBuffer();
		if ( !m_pBuffer )
			return false;

#ifndef DEDICATED
		// Force a very large memory buffer on the clients, this prevents peeking into the demo stream
		m_pBuffer->EnsureCapacity( DISK_DEMO_BUFFER_TOTAL_SIZE );
#endif

		// Demo files are always little endian
		m_pBuffer->SetBigEndian( false );
		m_bufDecoded.SetBigEndian( false );

		// Open the file
//		m_pBuffer->Open( pParams->pFilename, pParams->pszPath, pParams->nFlags, pParams->nOpenFileFlags );	// For main integration...
		m_pBuffer->Open( pParams->pFilename, pParams->pszPath, pParams->nFlags );
		m_nDecodedOffset = -1;

		m_pPlaybackParams = NULL;
#ifndef DEDICATED
		extern IDemoPlayer *demoplayer;
		extern IBaseClientDLL *g_ClientDLL;
		if ( demoplayer && g_ClientDLL )
		{
			m_pPlaybackParams = demoplayer->GetDemoPlaybackParameters();
		}
#endif

		return IsInitialized();
	}

	virtual void NotifySignonComplete() {}

	virtual void WriteHeader( void const *pData, int nSize )
	{
		// Byteswap
		demoheader_t littleEndianHeader = *((demoheader_t*)pData);
		ByteSwap_demoheader_t( littleEndianHeader );

		// Goto file start
		SeekPut( true, 0 );

		// Write
		Put( pData, nSize );
	}

	virtual void				NotifyBeginFrame() {}
	virtual void				NotifyEndFrame() {}

	virtual void				PutChar( char c )						{ m_pBuffer->PutChar( c ); }
	virtual void				PutUnsignedChar( unsigned char uc )		{ m_pBuffer->PutUnsignedChar( uc ); }
	virtual void				PutInt( int i )							{ m_pBuffer->PutInt( i ); }

	virtual void				WriteTick( int nTick )					{ m_pBuffer->PutInt( nTick ); }

	virtual char				GetChar() OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, sizeof( char ) );
		return readRequest.GetReadBuffer()->GetChar();
	}
	virtual unsigned char		GetUnsignedChar() OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, sizeof( unsigned char ) );
		return readRequest.GetReadBuffer()->GetUnsignedChar();
	}
	virtual int					GetInt() OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, sizeof( int ) );
		return readRequest.GetReadBuffer()->GetInt();
	}

	virtual void				Get( void* pMem, int size )	OVERRIDE
	{
		COnTheFlyDemoBufferReadInfo readRequest( m_pPlaybackParams, m_pBuffer, &m_bufDecoded, &m_nDecodedOffset, size );
		readRequest.GetReadBuffer()->Get( pMem, size );
	}
	virtual void				Put( const void* pMem, int size )
	{
		m_pBuffer->Put( pMem, size );

#ifndef DEDICATED
		if ( ( size > 0 ) && ( m_pBuffer->TellPut() > 0 ) &&
			( ( ( ( char* ) m_pBuffer->PeekPut() ) - ( ( char * ) m_pBuffer->Base() ) ) > (
				( GetBaseLocalClient().IsActive() && GetBaseLocalClient().ishltv ) ? 2048 : DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE
				) ) )
		{	// Periodically try to flush the demo buffer to disk
			m_pBuffer->TryFlushToFile( DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE );
		}
#endif
	}

	virtual bool				IsValid() const							{ return m_pBuffer && m_pBuffer->IsValid(); }
	virtual bool				IsInitialized() const					{ return IsValid() && m_pBuffer->IsOpen(); }

	inline CUtlBuffer::SeekType_t GetSeekType( bool bAbsolute )			{ return bAbsolute ? CUtlBuffer::SEEK_HEAD : CUtlBuffer::SEEK_CURRENT; }

	// Change where I'm writing (put)/reading (get)
	virtual void				SeekPut( bool bAbsolute, int offset )	{ m_pBuffer->SeekPut( GetSeekType( bAbsolute ), offset ); }
	virtual void				SeekGet( bool bAbsolute, int offset )	{ m_pBuffer->SeekGet( GetSeekType( bAbsolute ), offset ); }

	// Where am I writing (put)/reading (get)?
	virtual int					TellPut( ) const						{ return m_pBuffer->TellPut(); }
	virtual int					TellGet( ) const						{ return m_pBuffer->TellGet(); }

	virtual int					TellMaxPut( ) const						{ return m_pBuffer->TellMaxPut(); }

	virtual void				UpdateStartTick( int& nStartTick ) const {}
	virtual void				DumpToFile( char const* pFilename, const demoheader_t &header ) const {}

private:
	CUtlStreamBuffer *m_pBuffer;
	CUtlBuffer m_bufDecoded;
	int m_nDecodedOffset;
	CDemoPlaybackParameters_t const *m_pPlaybackParams;

	class COnTheFlyDemoBufferReadInfo
	{
	public:
		COnTheFlyDemoBufferReadInfo( CDemoPlaybackParameters_t const *pPlaybackParams, CUtlBuffer *pRawData, CUtlBuffer *pDecodeCache, int *pDecodedOffset, int numBytesRequired )
		{
			m_nReadFromBufferOriginalSeekPos = 0;
#ifndef DEDICATED
			if ( pPlaybackParams )
			{
				// Read from the nearest 16-byte aligned location
				int nOriginalGet = pRawData->TellGet();
				if ( ( (*pDecodedOffset) < 0 ) || // nothing decoded
					( nOriginalGet < (*pDecodedOffset) ) || // reading earlier
					( nOriginalGet + numBytesRequired > (*pDecodedOffset) + pDecodeCache->TellPut() ) ) // could read beyond decoded buffer
				{
					int nNearestAlignedLocation = nOriginalGet &~0xF;
					int blockRead = ( nOriginalGet + numBytesRequired - nNearestAlignedLocation + 0xF )&~0xF;
					blockRead = MAX( 1024, blockRead ); // decrypt chunks of 1K bytes at a time

					*pDecodedOffset = nNearestAlignedLocation;
					pDecodeCache->EnsureCapacity( blockRead );
					int numBytesSeekBack = nOriginalGet - nNearestAlignedLocation;
					if ( numBytesSeekBack )
						pRawData->SeekGet( pRawData->SEEK_CURRENT, - numBytesSeekBack ); // seek back

					int numBytes = MIN( blockRead, pRawData->TellMaxPut() - nNearestAlignedLocation );
					pRawData->Get( pDecodeCache->Base(), numBytes );
					m_nReadFromBufferOriginalSeekPos += -numBytes+numBytesSeekBack;
					pDecodeCache->SeekPut( pDecodeCache->SEEK_HEAD, numBytes );

					// Decode the chunk
					extern IBaseClientDLL *g_ClientDLL;
					g_ClientDLL->PrepareSignedEvidenceData( pDecodeCache->Base(), numBytes, pPlaybackParams );
				}

				int nSeekInDecodedBuffer = nOriginalGet - *pDecodedOffset;
				pDecodeCache->SeekGet( pDecodeCache->SEEK_HEAD, nSeekInDecodedBuffer );

				//
				// Set the read state
				//
				m_pReadFromBuffer = pDecodeCache;
				m_pSeekSyncBuffer = pRawData;
				m_nReadFromBufferOriginalSeekPos += -nSeekInDecodedBuffer;

				return;
			}
#endif

			//
			// Read raw state
			//
			m_pReadFromBuffer = pRawData;
			m_pSeekSyncBuffer = NULL;
		}
		~COnTheFlyDemoBufferReadInfo()
		{
			if ( m_pSeekSyncBuffer && m_pReadFromBuffer )
				m_pSeekSyncBuffer->SeekGet( m_pSeekSyncBuffer->SEEK_CURRENT, m_pReadFromBuffer->TellGet() + m_nReadFromBufferOriginalSeekPos );
		}

		CUtlBuffer * GetReadBuffer() const { return m_pReadFromBuffer; }
	private:
		CUtlBuffer *m_pReadFromBuffer;
		CUtlBuffer *m_pSeekSyncBuffer;
		int m_nReadFromBufferOriginalSeekPos;
	};
};




//-----------------------------------------------------------------------------
// Factory function
//-----------------------------------------------------------------------------
IDemoBuffer *CreateDemoBuffer( bool bMemoryBuffer, const DemoBufferInitParams_t& params )
{
	IDemoBuffer *pRet = static_cast< IDemoBuffer* >( new CDiskDemoBuffer() );

	if ( !pRet->Init( params ) )
	{
		delete pRet;
		return NULL;
	}

	return pRet;
}
