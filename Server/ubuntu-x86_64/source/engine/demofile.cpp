//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include <tier0/dbg.h>
#include <tier1/strtools.h>
#include <tier1/defer.h>
#include <utlbuffer.h>

#include "demofile.h"
#include "filesystem_engine.h"
#include "demo.h"
#include "demobuffer.h"
#include "convar.h"	// For dbg_demofile
#include "host_cmd.h"

#include "cdll_int.h" // For playback parameters

#include "netmessages.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


void Host_EndGame (bool bShowMainMenu, const char *message, ...);

// Temporary debug stuff:
#define DEMOFILE_DBG_PRINT
ConVar dbg_demofile( "dbg_demofile", "0" );
#if defined( DEMOFILE_DBG_PRINT )
class CDbgPrint
{
public:
	static int s_nIndent;
	CDbgPrint( const char *pMsg )
	{
		++s_nIndent;
		if ( dbg_demofile.GetInt() )
		{
			for (int i = 0; i < 3*s_nIndent; ++i)
				DevMsg(" ");
			DevMsg( "%s", pMsg );
		}
	}
	~CDbgPrint() { --s_nIndent; }
};
int CDbgPrint::s_nIndent = 0;
#define DemoFileDbg(_txt) CDbgPrint printer( _txt )
#else
#define DemoFileDbg(_txt) (void)0
#endif

ConVar demo_strict_validation( "demo_strict_validation", "0", FCVAR_RELEASE );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoFile::CDemoFile() :
	m_pBuffer( NULL )
{
}

CDemoFile::~CDemoFile()
{
	if ( IsOpen() )
	{
		Close();
	}
}

void CDemoFile::WriteSequenceInfo(int nSeqNrIn, int nSeqNrOut)
{
	DemoFileDbg( "WriteSequenceInfo()\n" );
	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutInt( nSeqNrIn );
	m_pBuffer->PutInt( nSeqNrOut );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::ReadSequenceInfo(int &nSeqNrIn, int &nSeqNrOut)
{
	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	nSeqNrIn = m_pBuffer->GetInt( );
	nSeqNrOut = m_pBuffer->GetInt( );
}


inline void ByteSwap_democmdinfo_t( democmdinfo_t &cmdInfo )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		democmdinfo_t::Split_t &swap = cmdInfo.u[ i ];

		swap.flags = LittleDWord( swap.flags );

		LittleFloat( &swap.viewOrigin.x, &swap.viewOrigin.x );
		LittleFloat( &swap.viewOrigin.y, &swap.viewOrigin.y );
		LittleFloat( &swap.viewOrigin.z, &swap.viewOrigin.z );

		LittleFloat( &swap.viewAngles.x, &swap.viewAngles.x );
		LittleFloat( &swap.viewAngles.y, &swap.viewAngles.y );
		LittleFloat( &swap.viewAngles.z, &swap.viewAngles.z );

		LittleFloat( &swap.localViewAngles.x, &swap.localViewAngles.x );
		LittleFloat( &swap.localViewAngles.y, &swap.localViewAngles.y );
		LittleFloat( &swap.localViewAngles.z, &swap.localViewAngles.z );

		LittleFloat( &swap.viewOrigin2.x, &swap.viewOrigin2.x );
		LittleFloat( &swap.viewOrigin2.y, &swap.viewOrigin2.y );
		LittleFloat( &swap.viewOrigin2.z, &swap.viewOrigin2.z );

		LittleFloat( &swap.viewAngles2.x, &swap.viewAngles2.x );
		LittleFloat( &swap.viewAngles2.y, &swap.viewAngles2.y );
		LittleFloat( &swap.viewAngles2.z, &swap.viewAngles2.z );

		LittleFloat( &swap.localViewAngles2.x, &swap.localViewAngles2.x );
		LittleFloat( &swap.localViewAngles2.y, &swap.localViewAngles2.y );
		LittleFloat( &swap.localViewAngles2.z, &swap.localViewAngles2.z );
	}
}

void CDemoFile::WriteCmdInfo( democmdinfo_t& info )
{
	DemoFileDbg( "WriteCmdInfo()\n" );
	democmdinfo_t littleEndianInfo = info;
	ByteSwap_democmdinfo_t( littleEndianInfo );

	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->Put( &littleEndianInfo, sizeof(democmdinfo_t) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::ReadCmdInfo( democmdinfo_t& info )
{
	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->Get( &info, sizeof(democmdinfo_t) );

	ByteSwap_democmdinfo_t( info );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//			*fp - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteCmdHeader( unsigned char cmd, int tick, int nPlayerSlot )
{
	if ( dbg_demofile.GetInt() ) DevMsg( "----------------------------------------\n" );
	Assert( cmd >= dem_signon && cmd <= dem_lastcmd );

	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutUnsignedChar( cmd );
	m_pBuffer->WriteTick( tick );
	Assert( nPlayerSlot >= 0 &&
			nPlayerSlot < MAX_SPLITSCREEN_CLIENTS );
	m_pBuffer->PutChar( nPlayerSlot );

	char *cmdname[] = 
	{
		"dem_unknown",
		"dem_signon",
		"dem_packet",
		"dem_synctick",
		"dem_consolecmd",
		"dem_usercmd",
		"dem_datatables",
		"dem_stop",
		"dem_customdata",
		"dem_stringtables"
	};

	DemoFileDbg( "WriteCmdHeader()..." );
	if ( dbg_demofile.GetInt() ) DevMsg( "tick %i, cmd %s \n", tick, cmdname[cmd] );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//			dt - 
//			frame - 
//-----------------------------------------------------------------------------
bool CDemoFile::ReadCmdHeader( unsigned char& cmd, int& tick, int &nPlayerSlot )
{
	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	cmd = m_pBuffer->GetUnsignedChar( );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		ConDMsg("Missing end tag in demo file.\n");
		cmd = dem_stop;
		return false;
	}

	if ( cmd <= 0 || cmd > dem_lastcmd )
	{
		ConDMsg("Unexepcted command token [%d] in .demo file\n", cmd );
		cmd = dem_stop;
		return false;
	}

	tick = m_pBuffer->GetInt( );
	nPlayerSlot = (int)m_pBuffer->GetChar();
	return true;
}

bool CDemoFile::DidReadPastEOF()
{
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
		return true;

	return false;
}

void CDemoFile::WriteConsoleCommand( const char *cmdstring, int tick, int nPlayerSlot )
{
	DemoFileDbg( "WriteConsoleCommand()\n" );
	if ( !cmdstring || !cmdstring[0] )
		return;

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return;

	int len = Q_strlen( cmdstring ) + 1;
	if ( len >= 1024 )
	{
		DevMsg("CDemoFile::WriteConsoleCommand: command too long (>1024).\n");
		return;
	}

	WriteCmdHeader( dem_consolecmd, tick, nPlayerSlot );

	WriteRawData( cmdstring, len );
}

const char *CDemoFile::ReadConsoleCommand()
{
	static char cmdstring[1024];
	
	ReadRawData( cmdstring, sizeof(cmdstring) );

	return cmdstring;
}

unsigned int CDemoFile::GetCurPos( bool bRead )
{
	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return 0;
	if ( bRead )
		return m_pBuffer->TellGet();
	return m_pBuffer->TellPut();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteNetworkDataTables( bf_write *buf, int tick )
{
	DemoFileDbg( "WriteNetworkDataTables()\n" );
	MEM_ALLOC_CREDIT();

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
	{
		DevMsg("CDemoFile::WriteNetworkDataTables: Haven't opened file yet!\n" );
		return;
	}

	WriteCmdHeader( dem_datatables, tick, 0 );

	WriteRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesWritten() );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : expected_length - 
//			&demofile - 
//-----------------------------------------------------------------------------
int CDemoFile::ReadNetworkDataTables( bf_read *buf )
{
	if ( buf )
		return ReadRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesLeft() );
	return ReadRawData( NULL, 0 ); // skip data
}

void CDemoFile::WriteStringTables( bf_write *buf, int tick )
{
	DemoFileDbg( "WriteStringTables()\n" );
	MEM_ALLOC_CREDIT();

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
	{
		DevMsg("CDemoFile::WriteStringTables: Haven't opened file yet!\n" );
		return;
	}

	WriteCmdHeader( dem_stringtables, tick, 0 );

	WriteRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesWritten() );
}

int CDemoFile::ReadStringTables( bf_read *buf )
{
	if ( buf )
		return ReadRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesLeft() );
	return ReadRawData( NULL, 0 ); // skip data
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmdnumber - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteUserCmd( int cmdnumber, const char *buffer, unsigned char bytes, int tick, int nPlayerSlot )
{
	DemoFileDbg( "WriteUserCmd()\n" );
	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return;

	WriteCmdHeader( dem_usercmd, tick, nPlayerSlot );

	m_pBuffer->PutInt( cmdnumber );

	WriteRawData( buffer, bytes );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : discard - 
//-----------------------------------------------------------------------------
int CDemoFile::ReadUserCmd( char *buffer, int &size )
{
	int outgoing_sequence;
	
	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	outgoing_sequence = m_pBuffer->GetInt();

	size = ReadRawData( buffer, size );
	return outgoing_sequence;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::WriteCustomData( int iCallbackIndex, const void *pData, size_t iDataSize, int tick )
{
	DemoFileDbg( "WriteCustomData()\n" );
	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return;

	WriteCmdHeader( dem_customdata, tick, 0 );

	//write the index of the callback according to the table we saved earlier
	m_pBuffer->PutInt( iCallbackIndex );

	MEM_ALLOC_CREDIT();

	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutInt( iDataSize );
	m_pBuffer->Put( pData, iDataSize );
}

//-----------------------------------------------------------------------------
// Purpose: Grabs the callback id and data for a custom data chunk
//-----------------------------------------------------------------------------
int CDemoFile::ReadCustomData( int *pCallbackIndex, uint8 **ppDataChunk )
{
	static CUtlVector<uint8> s_TempMemoryBuffer;
	int size;
	int iCallbackIndex;

	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	iCallbackIndex = m_pBuffer->GetInt();
	size = m_pBuffer->GetInt();

	if( (pCallbackIndex == NULL) || (ppDataChunk == NULL) )
	{
		//skip the chunk
		m_pBuffer->SeekGet( false, size );
		return 0;
	}

	s_TempMemoryBuffer.SetSize( size );
	*ppDataChunk = s_TempMemoryBuffer.Base();
	*pCallbackIndex = iCallbackIndex;

	// read data into buffer
	m_pBuffer->Get( s_TempMemoryBuffer.Base(), size );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		if ( demo_strict_validation.GetInt() )
			Host_EndGame(true, "Error reading demo message data.\n");
		return -1;
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Rewind from the current spot by the time stamp, byte code and frame counter offsets
//-----------------------------------------------------------------------------
void CDemoFile::SeekTo( int position, bool bRead )
{
	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	if ( bRead )
	{
		m_pBuffer->SeekGet( true, position );
	}
	else
	{
		m_pBuffer->SeekPut( true, position );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CDemoFile::ReadRawData( char *buffer, int length )
{
	int size;
	
	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	size = m_pBuffer->GetInt();

	if ( buffer && (length < size) )
	{
		DevMsg("CDemoFile::ReadRawData: buffer overflow (%i).\n", size );
		return -1;
	}

	if ( !buffer )
	{
		// just skip it
		m_pBuffer->SeekGet( false, size );
		return size;
	}

	if ( length < size )
	{
		// given buffer is too small
		DevMsg("CDemoFile::ReadRawData: buffer overflow (%i).\n", size );
		m_pBuffer->SeekGet( false, size );
		return -1;
	}

	// read data into buffer
	m_pBuffer->Get( buffer, size );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		if ( demo_strict_validation.GetInt() )
			Host_EndGame( true, "Error reading demo message data.\n" );
		return -1;
	}

	return size;
}

void CDemoFile::WriteRawData( const char *buffer, int length )
{
	DemoFileDbg( "WriteRawData()\n" );
	MEM_ALLOC_CREDIT();

	Assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutInt( length );
	m_pBuffer->Put( buffer, length );
}

void CDemoFile::WriteDemoHeader()
{
	DemoFileDbg( "WriteDemoHeader()\n" );
	Assert( m_DemoHeader.networkprotocol == GetHostVersion() );

	DevMsg( "\n" );
	DevMsg( "     demofilestamp: %s\n", m_DemoHeader.demofilestamp );
	DevMsg( "     demoprotocol (should be %i): %i\n", DEMO_PROTOCOL, m_DemoHeader.demoprotocol );
	DevMsg( "     networkprotocol (should be %i): %i\n", GetHostVersion(), m_DemoHeader.networkprotocol );
	DevMsg( "     servername: %s\n", m_DemoHeader.servername );
	DevMsg( "     clientname: %s\n", m_DemoHeader.clientname );
	DevMsg( "     mapname: %s\n", m_DemoHeader.mapname );
	DevMsg( "     gamedirectory: %s\n", m_DemoHeader.gamedirectory );
	DevMsg( "     playback_time: %f\n", m_DemoHeader.playback_time );
	DevMsg( "     playback_ticks: %i\n", m_DemoHeader.playback_ticks );	
	DevMsg( "     playback_frames: %i\n", m_DemoHeader.playback_frames );
	DevMsg( "     signonlength: %i\n", m_DemoHeader.signonlength );
	DevMsg( "\n" );

	// Swaps endianness, goes to file start and writes header
	m_pBuffer->WriteHeader( &m_DemoHeader, sizeof(demoheader_t) );
}

demoheader_t *CDemoFile::ReadDemoHeader( CDemoPlaybackParameters_t const *pPlaybackParameters )
{
	bool bOk;
	Q_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) );

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return NULL;
	m_pBuffer->SeekGet( true, pPlaybackParameters ? pPlaybackParameters->m_uiHeaderPrefixLength : 0 );
	m_pBuffer->Get( &m_DemoHeader, sizeof(demoheader_t) );
	bOk = m_pBuffer->IsValid();

	ByteSwap_demoheader_t( m_DemoHeader );

	if ( !bOk )
		return NULL;  // reading failed

	if ( Q_strcmp( m_DemoHeader.demofilestamp, DEMO_HEADER_ID ) )
	{
		ConMsg( "%s has invalid demo header ID.\n", m_szFileName );
		return NULL;
	}

	if ( ( m_DemoHeader.networkprotocol != GetHostVersion() ) &&
		( !pPlaybackParameters || !pPlaybackParameters->m_bAnonymousPlayerIdentity ) )
	{
		ConMsg ("WARNING: demo network protocol %i differs, engine version is %i \n", 
			m_DemoHeader.networkprotocol, GetHostVersion() );
	}

	if ( ( m_DemoHeader.demoprotocol > DEMO_PROTOCOL) ||
		 ( m_DemoHeader.demoprotocol < 2 ) )
	{
		ConMsg ("ERROR: demo file protocol %i outdated, engine version is %i \n", 
			m_DemoHeader.demoprotocol, DEMO_PROTOCOL );

		return NULL;
	}

	return &m_DemoHeader;
}

void CDemoFile::WriteFileBytes( FileHandle_t fh, int length )
{
	DemoFileDbg( "WriteFileBytes()\n" );
	int   copysize = length;
	char  copybuf[COM_COPY_CHUNK_SIZE];

	while ( copysize > COM_COPY_CHUNK_SIZE )
	{
		g_pFileSystem->Read ( copybuf, COM_COPY_CHUNK_SIZE, fh );
		m_pBuffer->Put( copybuf, COM_COPY_CHUNK_SIZE );
		copysize -= COM_COPY_CHUNK_SIZE;
	}

	g_pFileSystem->Read ( copybuf, copysize, fh );
	m_pBuffer->Put( copybuf, copysize );
	
	g_pFileSystem->Flush ( fh );
}

float CDemoFile::GetTicksPerSecond()
{
	return m_DemoHeader.playback_ticks / m_DemoHeader.playback_time;
}

float CDemoFile::GetTicksPerFrame()
{
	return m_DemoHeader.playback_ticks / m_DemoHeader.playback_frames;
}

int CDemoFile::GetTotalTicks( void )
{
	return m_DemoHeader.playback_ticks;
}

bool CDemoFile::Open( const char *name, bool bReadOnly, bool bMemoryBuffer )
{
	if ( m_pBuffer && m_pBuffer->IsInitialized() )
	{
		ConMsg( "CDemoFile::Open: file already open.\n" );
		return false;
	}

	m_szFileName[0] = 0;  // clear name
	Q_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) ); // and demo header

	if ( bMemoryBuffer )
	{
		Assert( !bReadOnly );	// Only read from files
		int const nMaxSize = 1024 * 1024 * 16;
		MemoryDemoBufferInitParams_t params( nMaxSize );
		m_pBuffer = CreateDemoBuffer( true, params );
	}
	else
	{
		StreamDemoBufferInitParams_t params( name, NULL, bReadOnly ? CUtlBuffer::READ_ONLY : 0, IsX360() ? FSOPEN_NEVERINPACK : 0 );
		m_pBuffer = CreateDemoBuffer( false, params );
	}

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
	{
		ConMsg( "CDemoFile::Open: couldn't open file %s for %s.\n", 
			name, bReadOnly ? "reading" : "writing" );
		Close();
		return false;
	}

	Q_strncpy( m_szFileName, name, sizeof(m_szFileName) );
	return true;
}

bool CDemoFile::IsOpen()
{
	return m_pBuffer && m_pBuffer->IsInitialized();
}

void CDemoFile::Close()
{
	delete m_pBuffer;
	m_pBuffer = NULL;
}

int CDemoFile::GetSize()
{
	return m_pBuffer->TellMaxPut();
}

void CDemoFile::DumpBufferToFile( char const* pFilename, const demoheader_t &header )
{
	// TODO: Fix this crash - when user tries to download before replay_record has been run.
	m_pBuffer->DumpToFile( pFilename, header );
}

void CDemoFile::UpdateStartTick( int& nStartTick )
{
	if ( m_pBuffer )
	{
		m_pBuffer->UpdateStartTick( nStartTick );
	}
}

void CDemoFile::NotifySignonComplete()
{
	m_pBuffer->NotifySignonComplete();
}

void CDemoFile::NotifyBeginFrame()
{
	m_pBuffer->NotifyBeginFrame();
}

void CDemoFile::NotifyEndFrame()
{
	m_pBuffer->NotifyEndFrame();
}

static bool DemoFixParseSignonPacket( bf_read& data, float* pIntervalPerTick )
{
	while ( data.GetNumBytesLeft() > 0 )
	{
		int type = data.ReadVarInt32();

		if ( type != CSVCMsg_ServerInfo_t::sk_Type )
		{
			int len = data.ReadVarInt32();
			if ( len > data.GetNumBytesLeft() )
				return false;

			data.SeekRelative( len * 8 ); // bf_read is in bits
			continue;
		}

		CSVCMsg_ServerInfo_t msg;
		if ( !msg.ReadFromBuffer( data ) )
			return false;

		if ( msg.has_tick_interval() )
			*pIntervalPerTick = msg.tick_interval();
	}

	return true;
}

bool AttemptToFixDemoFile( const char* filename )
{
	CDemoFile demo;

	if ( !demo.Open( filename, true ) )
		return false;

	demoheader_t* pHeader = demo.ReadDemoHeader( nullptr );
	if ( !pHeader )
		return false;

	// Read packets until we get to the end of the file, then rewind to the last valid packet
	bool bSignonComplete = false;
	int signonSize = 0;
	bool bReadStop = false;

	int lastValidPos = demo.GetCurPos( true );
	int numTicks = 0;
	int numFrames = 0;
	float interval_per_tick = 1.0f / 64.0f; // will usually overridden by SVCMsg_ServerInfo during signon
	CUtlBuffer dataBuffer( 0, NET_MAX_PAYLOAD ); // too big for stack

	for(;;)
	{
		unsigned char cmd;
		int tick;
		int playerSlot;

		// Read next command
		if ( !demo.ReadCmdHeader( cmd, tick, playerSlot ) )
			break;

		if ( !bSignonComplete && ( cmd == dem_packet || cmd == dem_synctick ) )
		{
			bSignonComplete = true;
			signonSize = lastValidPos - sizeof( demoheader_t );
		}

		switch ( cmd )
		{
		case dem_signon:
		case dem_packet:
		{
			democmdinfo_t cmdInfo;
			demo.ReadCmdInfo( cmdInfo );

			int seqNrIn, seqNrOut;
			demo.ReadSequenceInfo( seqNrIn, seqNrOut );

			if ( cmd == dem_signon && !bSignonComplete )
			{
				// Look for CSVCMsg_ServerInfo and extract tickrate
				int len = demo.ReadRawData( (char*)dataBuffer.Base(), dataBuffer.Size() );
				if ( len < 0 )
					return false;

				bf_read data( dataBuffer.Base(), len );
				if ( !DemoFixParseSignonPacket( data, &interval_per_tick ) )
					return false;
			}
			else
			{
				// skip actual packet data
				demo.ReadRawData( nullptr, 0 );
			}

			break;
		}

		case dem_synctick:
			// no data
			break;

		case dem_consolecmd:
			demo.ReadConsoleCommand();
			break;

		case dem_usercmd:
		{
			int cmdsize;
			demo.ReadUserCmd( nullptr, cmdsize );
			break;
		}

		case dem_datatables:
			demo.ReadNetworkDataTables( nullptr );
			break;

		case dem_stop:
			// no data
			break;

		case dem_customdata:
			demo.ReadCustomData( nullptr, nullptr );
			break;

		case dem_stringtables:
			demo.ReadStringTables( nullptr );
			break;
		}

		if( demo.DidReadPastEOF() )
		{
			break;
		}

		lastValidPos = demo.GetCurPos( true );

		if ( tick > numTicks )
			numTicks = tick;

		if ( cmd == dem_stop )
		{
			bReadStop = true;
			break;
		}

		if ( cmd == dem_packet )
		{
			numFrames++;
		}
	}

	bool bFixHeader = ( pHeader->playback_frames != numFrames );
	demoheader_t header = *pHeader;
	demo.Close();

	if ( bSignonComplete && !bFixHeader && bReadStop )
	{
		// demo is ok
		ConMsg( "Demo is OK, no fixes applied\n" );
		return true;
	}

	// Can't fix a demo with unfinished signon
	if ( !bSignonComplete )
		return false;

	// Open the demo again
	// Unfortunately it's difficult to open a CDemoFile for update
	// so we duplicate some functionality here (in particular, 
	// we implement WriteDemoHeader() and WriteCmdHeader() on
	// raw FileHandle_t instead of a CUtlBuffer)

	FileHandle_t hFileHandle = g_pFullFileSystem->OpenEx( filename, "r+b" );
	if ( !hFileHandle )
		return false;
	DEFER( { g_pFullFileSystem->Close( hFileHandle ); } ); // Close the file automatically on return

	if ( bFixHeader )
	{
		// Fix up header if it looks wrong
		header.signonlength = signonSize;
		header.playback_frames = numFrames;
		header.playback_ticks = numTicks;
		header.playback_time = numTicks * interval_per_tick;

		ByteSwap_demoheader_t( header );
		g_pFullFileSystem->Seek( hFileHandle, 0, FILESYSTEM_SEEK_HEAD );
		int headerSize = g_pFullFileSystem->Write( &header, sizeof( demoheader_t ), hFileHandle );
		if ( headerSize != sizeof( demoheader_t ) )
			return false;
	}

	if ( !bReadStop )
	{
		// add dem_stop at end of file
		g_pFullFileSystem->Seek( hFileHandle, lastValidPos, FILESYSTEM_SEEK_HEAD );
		unsigned char cmd = dem_stop;
		int tick = LittleDWord( numTicks );
		unsigned char slot = 0;

		char buf[ sizeof(cmd) + sizeof(tick) + sizeof(slot) ];
		int bufpos = 0;
		memcpy( &buf[bufpos], &cmd,			sizeof( cmd ) );		bufpos += sizeof( cmd );
		memcpy( &buf[bufpos], &tick,		sizeof( tick ) );		bufpos += sizeof( tick );
		memcpy( &buf[bufpos], &slot,		sizeof( slot ) );		bufpos += sizeof( slot );
		Assert( bufpos == sizeof( buf ) );

		int sizeWritten = g_pFullFileSystem->Write( buf, sizeof( buf ), hFileHandle );
		if ( sizeWritten != sizeof( buf ) )
			return false;

		// Truncate file
		if ( !g_pFullFileSystem->Truncate( hFileHandle ) )
			return false;
	}

	return true;
}
