//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#if !defined( SOURCE2_PANORAMA )
#include "steamcommon.h"
#endif
#include "controls/debug/debugger.h"
//#include "controls/debug/console.h"
#ifdef WIN32
#if !defined( SOURCE2_PANORAMA )
#include "text/uitextlayoutwin32.h"
#endif
#if defined( OGL ) && !defined( SOURCE2_PANORAMA )
#include "text/uitextlayoutposix.h"
#include "uienginesdl.h"
#endif

#if !defined( SOURCE2_PANORAMA )
#include "uienginewin32.h"
#endif

#endif // WIN32
#if defined( SOURCE2_PANORAMA )
#include "source2/uienginesource2.h"
#include "appframework/iapplication.h"
#endif

#if !defined( SOURCE2_PANORAMA )
static CCommandLineParam g_DevMode( "-dev", "Developer mode" );
static CCommandLineParam g_WebBrowserOS( "-webplatform", "Used to test web requests as a different OS" );
#endif

#ifdef POSIX
#if !defined( SOURCE2_PANORAMA )
#include "text/uitextlayoutposix.h"
#include "../common/clientdirectories.h"
#include "uienginesdl.h"
#endif
#endif

#include "uijsregistration.h"
#if !defined( SOURCE2_PANORAMA )
#include "audio/iaudiointerface.h"
#include "uitoplevelwindowopenvroverlay.h"
#include <vrapi.h>
#endif

#if !defined( PANORAMA_DISABLE_VIDEO )
#include "video/ivideoplayer.h"
#endif

#if defined( SOURCE2_PANORAMA )
#include "tier0/icommandline.h"
#include "v8/include/libplatform/libplatform.h"
#else
#include "../external/v8/include/libplatform/libplatform.h"
#endif

#if !defined( SOURCE2_PANORAMA ) 
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
#define ClientUtils SteamUtils
#define ClientHTTP SteamHTTP
#endif
#endif

#if ( DEVELOPMENT_ONLY )
#define PANORAMA_EVENT_STATS_ENABLED 1
#endif

#if V8_DEBUGGING_ENABLED

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

#include <v8/include/v8-inspector.h>

#endif	// V8_DEBUGGING_ENABLED

#include "uisoundsystem.h"
#include "memstack.h"

#include "tier1/utldelegate.h"

#include "ctx_debug.h"

#ifdef PANORAMA_USE_S1WRAPPER
// There is a complete clustermess about how S1 engine can be accessed from panorama.dll
// adding a few #defines to prevent including headers that cause compile errors, this is a total hack
#define RESOURCESTREAM_H
#define RESOURCEFILE_H
#define RESOURCETYPE_H
#include "cdll_int.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#if V8_DEBUGGING_ENABLED

void OnPanoramaRemoteDebug( IConVar *var, const char *pOldValue, float flOldValue );
ConVar panorama_remote_debug( "panorama_remote_debug", "1", FCVAR_DEVELOPMENTONLY, "Enable ability to connect to process via panorama remote debugger.", OnPanoramaRemoteDebug );
ConVar panorama_remote_debug_trace( "panorama_remote_debug_trace", "0", FCVAR_DEVELOPMENTONLY, "Enable ability to print message traffic between V8 and remote debugger." );
ConVar panorama_dbg_brkpts_timeout_ms( "panorama_dbg_brkpts_timeout_ms", "500", FCVAR_DEVELOPMENTONLY, "Wait before running script to give the debug Front End time to set breakpoints" );

#define V8_Msg( ... ) do { if (panorama_remote_debug_trace.GetBool()) Msg( ##__VA_ARGS__ );  } while( 0 )

#define JS_STACK_FRAME_LEN		512
#define JS_STACK_FRAME_COUNT	64
char g_JSStackTrace[ JS_STACK_FRAME_COUNT ][JS_STACK_FRAME_LEN+1];

namespace panorama
{
	IUIEngine *UIEngine();
}

//-----------------------------------------------------------------------------
// CWebsocketServer
//	Communication with front ends like Visual Studio Code or CDT.
//
//  CWebsocketServer::RunFrame polls the underlying websocket and sends any 
//	messages received from FE on to v8 (inspector). v8 replies immediately, which
//	reply is sent back to the FE, and in response more messages can be received from the 
//	FE, to which v8 replies immediately and so on...all in a single call to 
//	server::poll_one() inside CWebsocketServer::RunFrame(). 
//
//	There can be a break in this conversation, though, so there is no guarantee 
//	that a single call to server::poll_one() is actually enough to complete a 
//	logical action such as setting a breakpoint, which involves a series of messages. 
//	Hence the functionality to call poll_one repeatedly with various options that 
//	are arguments to CWebsocketServer::RunFrame().
//-----------------------------------------------------------------------------

class InspectorClient;

class CWebsocketServer
{
public:

	typedef websocketpp::server<websocketpp::config::asio> server;

	CWebsocketServer( );
	~CWebsocketServer();

	bool Start( uint16_t port, InspectorClient *pInspectorClient );
	void Stop();

	// RunFrame - runs for minTimeMS, which includes at least one poll of underlying
	// transport, after which optionally block until atleast one message received from front end
	// (which could be a message received in inital poll) or until timeout. See note in 
	// CWebsocketServer header comment
	void RunFrame( uint32_t minTimeMS = 0, bool bBlockTillMsgRcvdFromFE = false, uint32_t blockTimeoutMS = 0xFFFFFFFF );

	void OnConnect(connection_hdl hdl);
	void OnDisconnect( connection_hdl hdl );

	void RcvMessageFromFE(connection_hdl hdl, server::message_ptr msg);
	void SendMessageToFE( const char *pMsg );

	// Must send response to initial HTTP handshake before upgrade to Websocket can happen
	void OnHTTP( websocketpp::connection_hdl hdl );

	bool IsConnected() { return ( m_connection.lock() != nullptr) ; }

private:

	 server m_server;
	 connection_hdl m_connection;
	 InspectorClient *m_pInspectorClient;
	 bool m_bInDisconnectHandler = false;
	 bool m_bRcvdMsgFromFrontEndThisFrame = false;
};

// Implementation further down in this file, at point where InspectorClient 
// definition is available

//-----------------------------------------------------------------------------
// InspectorChannel
//	Receives messages from V8, passes them on to the server to send to the FE
//-----------------------------------------------------------------------------

enum 
{
  // The debugger reserves the first slot in the Context embedder data.
  kDebugIdIndex = v8::Context::kDebugIdIndex,
  kModuleEmbedderDataIndex,
  kInspectorClientIndex
};

class InspectorChannel final : public v8_inspector::V8Inspector::Channel 
{
public:

	InspectorChannel( CWebsocketServer *pServer ):
		m_pServer( pServer ) {}

	void RunServerFrame()
	{
		m_pServer->RunFrame();
	}

private:
  
	void sendResponse( int callId, std::unique_ptr<v8_inspector::StringBuffer> message) override 
	{
		SendToFE(message->string());
	}

	void sendNotification( std::unique_ptr<v8_inspector::StringBuffer> message) override 
	{
		SendToFE(message->string());
	}

	void flushProtocolNotifications() override {}

	void SendToFE(const v8_inspector::StringView& string) 
	{
		int len = string.length();
		char *str = new char[ len + 1 ];
		const uint8_t *strSrc = string.characters8();

		for ( int i = 0; i < len; i++ )
		{
			str[i] = *strSrc;
			strSrc += 2;
		}

		str[ len ] = 0;
		V8_Msg( "V8 to FrontEnd: %s\n", str );

		m_pServer->SendMessageToFE( str );
		
		delete [] str;
	}

	class CWebsocketServer *m_pServer;
};

//-----------------------------------------------------------------------------
// InspectorClient
//	Sends messages to V8
//-----------------------------------------------------------------------------

class InspectorClient : public v8_inspector::V8InspectorClient 
{
public:
	InspectorClient( v8::Isolate *pIsolate, CWebsocketServer *pServer ) 
	{
		m_pIsolate = pIsolate;
		channel_.reset( new InspectorChannel( pServer ) );
		inspector_ = v8_inspector::V8Inspector::create( m_pIsolate, this);
		session_ =	inspector_->connect(1, channel_.get(), v8_inspector::StringView());
	}

	~InspectorClient()
	{
		// Nothing to do, resources deleted automatically by unique_ptrs going out of scope
	}

	void SendInspectorMessage( const char * str ) 
	{
		V8_Msg( "FrontEnd to V8: %s\n", str );
		v8_inspector::StringView message_view( (const uint8_t*)str, strlen(str) );

		v8::Isolate::Scope isolate_scope( m_pIsolate );
		session_->dispatchProtocolMessage(message_view);
	}

	void ContextCreated( v8::Local< v8::Context > context )
	{
		inspector_->contextCreated( 
			v8_inspector::V8ContextInfo( context, kContextGroupId, v8_inspector::StringView( (uint8_t*)"CSGO Context", 12 )));
	}

	void ContextDestroyed( v8::Local< v8::Context > context )
	{
		inspector_->contextDestroyed( context );
	}

	// Must unpause V8 if debugger disconnects inside pause loop
	void OnDisconnect()
	{
		SendInspectorMessage("{\"id\":0,\"method\":\"Debugger.disable\"}");
		quitMessageLoopOnPause();
	}

private:

	virtual void runMessageLoopOnPause( int contextGroupId ) override
	{
		V8_Msg("V8 Running message loop on pause\n");
		m_bMsgLoopOnPause = true;

		while ( m_bMsgLoopOnPause )
		{	
			channel_->RunServerFrame();
		}
	}

	virtual void quitMessageLoopOnPause() override
	{
		V8_Msg("V8 Stopping message loop on pause\n");
		m_bMsgLoopOnPause = false;
	}

	static const int kContextGroupId = 1;

	volatile bool m_bMsgLoopOnPause = false;
	v8::Isolate *m_pIsolate;
	std::unique_ptr<v8_inspector::V8Inspector> inspector_;
	std::unique_ptr<v8_inspector::V8InspectorSession> session_;
	std::unique_ptr<InspectorChannel> channel_;
};

//-----------------------------------------------------------------------------
// CWebsocketServer implementation
//-----------------------------------------------------------------------------

CWebsocketServer::CWebsocketServer( )
{
    m_server.init_asio();

	m_server.set_http_handler( bind(&CWebsocketServer::OnHTTP, this, ::_1) );
    m_server.set_open_handler( bind(&CWebsocketServer::OnConnect, this, ::_1) );
    m_server.set_close_handler( bind(&CWebsocketServer::OnDisconnect, this, ::_1) );
    m_server.set_message_handler( bind(&CWebsocketServer::RcvMessageFromFE, this, ::_1, ::_2) );
}

CWebsocketServer::~CWebsocketServer()
{
	Stop();
}

bool CWebsocketServer::Start( uint16_t port, InspectorClient *pInspectorClient )
{
	m_pInspectorClient = pInspectorClient;
	websocketpp::lib::error_code ec;		
	m_server.listen(port, ec);
	if ( ec.value() != 0 )
	{
		Msg( "V8 debugger error: %s. Another instance already running?\n", ec.message().c_str() );
		return false;
	}
    m_server.start_accept();
	return true;
}

void CWebsocketServer::Stop()
{
	websocketpp::lib::error_code ec;
	m_server.stop_listening( ec);		

	if ( m_connection.lock() != nullptr )
	{
		server::connection_ptr con = m_server.get_con_from_hdl( m_connection );
		con->close( websocketpp::close::status::going_away, "Goodbye", ec );
		m_connection.reset();
	}
}

void CWebsocketServer::RunFrame( uint32_t minTimeMS /*= 0*/, bool bBlockTillMsgRcvdFromFE /*= false*/, uint32_t blockTimeoutMS /*= 0xFFFFFFFF*/ )
{
	// One call to poll_one takes care of an entire exchange of messages
	m_bRcvdMsgFromFrontEndThisFrame = false;

	uint32_t startTime = Plat_MSTime();

	// Block till min time is done
	do
	{
		m_server.poll_one();
	} while ( ( Plat_MSTime() - startTime ) < minTimeMS );
	
	if ( bBlockTillMsgRcvdFromFE )
	{
		// Reset start time
		startTime = Plat_MSTime();

		while ( !m_bRcvdMsgFromFrontEndThisFrame )
		{
			m_server.poll_one();

			if ( ( Plat_MSTime() - startTime ) > blockTimeoutMS )
			{
				break;
			}
		}
	}
}

void CWebsocketServer::OnConnect( connection_hdl hdl )
{
	if ( m_connection.lock() != nullptr )
	{
		V8_Msg("V8 debugger recived connection request, but is already connected. Ignoring.\n");
		return;
	}

	Msg("V8 debugger connected.\n");		
	m_connection = hdl.lock();
	panorama::UIEngineInternal()->DebuggerFrontEndConnected();

}

void CWebsocketServer::OnDisconnect( connection_hdl hdl )
{
	// Don't shut down server here, that will be done by CUIEngine 
	// in it's frame loop
	m_connection.reset();
	m_pInspectorClient->OnDisconnect();
	panorama::UIEngineInternal()->DebuggerFrontEndDisconnected();
	Msg("V8 debugger disconnected.\n");
}

void CWebsocketServer::RcvMessageFromFE(connection_hdl hdl, server::message_ptr msg)
{
	m_bRcvdMsgFromFrontEndThisFrame = true;

	m_pInspectorClient->SendInspectorMessage( msg->get_payload().c_str() );
}

void CWebsocketServer::SendMessageToFE( const char *pMsg )
{
	if ( m_connection.lock() != nullptr)
	{
		m_server.send( m_connection, pMsg, websocketpp::frame::opcode::text );
	}
}

void CWebsocketServer::OnHTTP( websocketpp::connection_hdl hdl )
{
	server::connection_ptr con = m_server.get_con_from_hdl( hdl );

	con->append_header( "Content-Type", "application/json; charset=UTF-8" );

	std::stringstream output;
	output <<	"[{"
				"\"description\":\"node.js instance\", "
				"\"webSocketDebuggerUrl\":\"ws://127.0.0.1:42000\", "
				"\"id\":\"57f36f1a-6c6f-4148-af1a-802bf5c23c7f\", "
				"\"title\":\"app.js\", "
				"\"url\":\"file://D:_work_hello_app.js\", "
				"\"type\":\"node\""
			"}]";
    
	// Set status to 200 rather than the default error code
	con->set_status(websocketpp::http::status_code::ok);

	// Set body text to the HTML created above
	con->set_body(output.str());
}

#endif // V8_DEBUGGING_ENABLED

ConVar panorama_dump_events_backlog( "panorama_dump_events_backlog", "0", FCVAR_RELEASE );

using namespace panorama;

// TODO: Change this to be address of global context
const uint64 GLOBAL_CONTEXT_SEC_TOKEN = 0xFEEDBEEFFEEDBEEF;

uint64 GetContextSecurityToken( v8::Context *pContext )
{
	uint64 secTokenInt = 0;

	v8::Local<v8::Value> secToken = pContext->GetSecurityToken();
	if ( secToken->IsString() )
	{
		v8::String::Utf8Value source(v8::Isolate::GetCurrent(),  secToken );
		secTokenInt = V_strtoui64( *source, NULL, 16 );
	}
	else
	{
		V8_CtxDbgAssert( "Context has invalid security token" );
	}

	return secTokenInt;
}

void SetContextSecurityToken( v8::Isolate *pIsolate, v8::Context *pContext, uint64 secToken )
{
	CFmtStr1024 strToken( "%llx", secToken );
	const char* tokenAsStr = strToken.String();

	v8::Handle<v8::String> securityToken;
	securityToken = v8::String::NewFromUtf8( pIsolate, tokenAsStr ).ToLocalChecked();
	
	pContext->SetSecurityToken( securityToken );

	if ( GetContextSecurityToken( pContext ) != secToken )
	{
		V8_CtxDbgAssert( "Security token mismatch" );
	}
}

namespace panorama
{
	ConVar g_cvarDeveloper( "developer", IsDebug()?"1":"0" );

	DECLARE_PANORAMA_EVENT4( JSScheduledFunction, CPanelPtr<IUIPanel>, v8::Persistent<v8::Function> *, int, uint );
	DEFINE_PANORAMA_EVENT( JSScheduledFunction );

	DECLARE_PANORAMA_EVENT0( ReloadChangedUIFiles );
	DEFINE_PANORAMA_EVENT( ReloadChangedUIFiles );

#if DEVELOPMENT_ONLY
	DECLARE_PANORAMA_EVENT0( ReloadPanorama );
	DEFINE_PANORAMA_EVENT( ReloadPanorama )

	DECLARE_PANORAMA_EVENT0( ForceReloadPanorama );
	DEFINE_PANORAMA_EVENT( ForceReloadPanorama )
#endif

	IUIEngine *g_IUIEngine = NULL;
	IUIEngine *UIEngine() { return g_IUIEngine; }
	IUILocalization *UILocalize() { return UIEngine()->UILocalize(); }
	IUISoundSystem *UISoundSystem() { return UIEngine()->UISoundSystem(); }
	IUIInput *UIInputEngine() { return UIEngine() ? UIEngine()->UIInputEngine() : NULL; }

#if !defined( SOURCE2_PANORAMA ) 
	IUITextServices *g_IUITextServices;
#endif
	IUITextServices *UITextServices() { return g_IUITextServices; }

	ConVar g_cvarHTTPCacheSize( "http_cache_size", "150" );
	ConVar g_consoleHistorySize( "console_history_size", "1000" );

	const int k_nTargetSleepFrameRate = 10; // the FPS we want the CEF html thread to run at when we are not actively rendering

	ConVar s_convarMaxFPS( "@panorama_max_fps", "120.0f" );
	ConVar s_convarMaxOverlayFPS( "@panorama_max_overlay_fps", "60.0f" );
	ConVar s_convarOutOfFocusMaxFPS( "@panorama_max_oof_overlay_up_fps", "4.0f" );
	ConVar s_convarLargeDispatchEventQueue( "@panorama_large_dispatch_event_queue", "0" );

#ifdef PANORAMA_EVENT_STATS_ENABLED

	static void OnEventStatsChange( IConVar *var, const char *pOldValue, float flOldValue );
	static ConVar s_convarPanoramaEventStats( "@panorama_event_stats", "0", FCVAR_DEVELOPMENTONLY, "", OnEventStatsChange );
	static ConVar s_convarPanoramaEventStatsPosX( "@panorama_event_stats_posx", "230", FCVAR_DEVELOPMENTONLY );
	static ConVar s_convarPanoramaEventStatsPosY( "@panorama_event_stats_posy", "810", FCVAR_DEVELOPMENTONLY );

	struct EventFrameStats_t
	{
		uint m_nEvents;
		CCycleCount m_cycleCount;

		void InitZero()
		{
			m_nEvents = 0;
			m_cycleCount = 0;
		}
		void Increment( CCycleCount const&eventCycleCount )
		{
			m_nEvents++;
			m_cycleCount += eventCycleCount;
		}
	};

	struct EventStatsTracker_t
	{
		enum
		{
			NUM_SAMPLES = 400
		};

		int m_nSampleIdx;
		EventFrameStats_t m_Samples[NUM_SAMPLES];
		EventFrameStats_t m_min;
		EventFrameStats_t m_max;
		EventFrameStats_t m_tot;
		EventFrameStats_t m_avg;
		EventFrameStats_t m_cur;

		void InitZero()
		{
			V_memset( this, 0, sizeof( *this ) );
		}

		void AddSample( const EventFrameStats_t &sample )
		{
			m_cur = sample;
			m_Samples[m_nSampleIdx] = sample;
			m_nSampleIdx = (m_nSampleIdx + 1) % NUM_SAMPLES;

			// Recompute min, max and total
			m_min = sample;
			m_max = sample;
			m_tot.InitZero();
			m_avg.InitZero();

			for( int nSample = 0; nSample < NUM_SAMPLES; ++nSample )
			{
				const EventFrameStats_t &current = m_Samples[nSample];

				m_min.m_nEvents = MIN( m_min.m_nEvents, current.m_nEvents );
				m_max.m_nEvents = MAX( m_max.m_nEvents, current.m_nEvents );
				m_tot.m_nEvents += current.m_nEvents;

				if( current.m_cycleCount.IsLessThan( m_min.m_cycleCount ) )
				{
					m_min.m_cycleCount = current.m_cycleCount;
				}

				if( m_max.m_cycleCount.IsLessThan( current.m_cycleCount ) )
				{
					m_max.m_cycleCount = current.m_cycleCount;
				}

				m_tot.m_cycleCount += current.m_cycleCount;
			}

			m_avg.m_nEvents = m_tot.m_nEvents / NUM_SAMPLES;
			m_avg.m_cycleCount.m_Int64 = m_tot.m_cycleCount.m_Int64 / NUM_SAMPLES;
		}

	};

	// Totals for all event types
	static EventFrameStats_t s_totalFrameStats;
	static EventStatsTracker_t s_totalTracker;

	// per-event tracking
	struct PerEventStats_t
	{
		PerEventStats_t()
		{
			m_frameStats.InitZero(); 
			m_tracker.InitZero();
		}

		CPanoramaSymbol m_type;
		EventFrameStats_t m_frameStats;
		EventStatsTracker_t m_tracker;
	};

	static CUtlMap< CPanoramaSymbol, PerEventStats_t*, int, CDefLess< CPanoramaSymbol > > s_perEventStatsMap;
	static CUtlBlockVector<PerEventStats_t> s_perEventStats;
	static CUtlVector<PerEventStats_t*> s_sortedEventStats;

	static void OnEventStatsChange( IConVar *var, const char *pOldValue, float flOldValue )
	{
		int nVal = s_convarPanoramaEventStats.GetInt();
		if( nVal > 0 )
		{
			s_totalFrameStats.InitZero();
			s_totalTracker.InitZero();
			s_perEventStatsMap.Purge();
			s_perEventStats.Purge();
			s_sortedEventStats.Purge();
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Sort comparator
	//-----------------------------------------------------------------------------
	inline int CompareEventAvgTimes( PerEventStats_t *const *pplhs, PerEventStats_t *const *pprhs )
	{
		EventStatsTracker_t &lTracker = (*pplhs)->m_tracker;
		EventStatsTracker_t &rTracker = (*pprhs)->m_tracker;
		return (rTracker.m_max.m_cycleCount.m_Int64 - lTracker.m_max.m_cycleCount.m_Int64);
	}

	static void EventStatsFrameUpdate()
	{
		int nVal = s_convarPanoramaEventStats.GetInt();
		if( nVal > 0 )
		{
			// Update per-frame stats with data from previous frame
			s_totalTracker.AddSample( s_totalFrameStats );

			if( nVal > 1 )
			{
				s_sortedEventStats.RemoveAll();
				s_sortedEventStats.EnsureCapacity( s_perEventStats.Count() );
				FOR_EACH_VEC( s_perEventStats, i )
				{
					s_perEventStats[i].m_tracker.AddSample( s_perEventStats[i].m_frameStats );
					s_sortedEventStats.AddToTail( &s_perEventStats[i] );
				}
				s_sortedEventStats.Sort( &CompareEventAvgTimes );
			}

			// Reset counters
			s_totalFrameStats.InitZero();
			FOR_EACH_VEC( s_perEventStats, i )
			{
				s_perEventStats[i].m_frameStats.InitZero();
			}
		}
	}

	static void EventStatsUpdate( CPanoramaSymbol eventType, CCycleCount const&eventCycleCount )
	{
		s_totalFrameStats.Increment( eventCycleCount );

		if( s_convarPanoramaEventStats.GetInt() > 1 )
		{
			PerEventStats_t *pStats = NULL;

			int iMap = s_perEventStatsMap.Find( eventType );
			if( iMap == s_perEventStatsMap.InvalidIndex() )
			{
				int iVec = s_perEventStats.AddToTail();
				pStats = &s_perEventStats.Tail();
				pStats->m_type = eventType;
				iMap = s_perEventStatsMap.Insert( eventType, pStats );
			}
			else
			{
				pStats = s_perEventStatsMap.Element( iMap );
			}
			pStats->m_frameStats.Increment( eventCycleCount );
		}
	}

	static void DrawDebugBg( CUIRenderEngine * pRenderEngine, float x0, float y0, float width, float height )
	{
		const Color bgColor( 0, 0, 0, 210 );
		pRenderEngine->DrawSolidColorRect( x0, y0, x0 + width, y0 + height, bgColor.AsUint32(), k_EAntialiasingNone );
	}

	static void DrawDebugText( CUIRenderEngine * pRenderEngine, const char *pchText, float x0, float y0, float fontSize )
	{
		if( pRenderEngine )
		{
			const Color redColor( 255, 0, 0, 255 );

			pRenderEngine->DrawSolidColorTextRegion( pchText, "Arial Unicode MS", redColor.AsUint32(), fontSize, k_flFloatNotSet,
				k_EFontWeightBold, k_EFontStyleNormal, k_ETextAlignLeft, k_ETextDecorationNone,
				false, false, 1, x0, y0, x0 + 300.0f, y0 + 50.0f );
		}
	}

	static void EventStatsDraw( CUIRenderEngine * pRenderEngine )
	{
		int nConVar = s_convarPanoramaEventStats.GetInt();

		if( nConVar > 0 )
		{
			pRenderEngine->PushAnimationAndTransformContext( 0, 0, 0, 1000, 1000, NULL, true, false, k_EPanelRepaintFull, false, false, false, false, true, false, nullptr, false, false, false, false, k_EFractionalPixelPositionsDefault );

			static CFmtStr1024 s_fmtStr;
			const float xpos = s_convarPanoramaEventStatsPosX.GetFloat();
			const float ypos = s_convarPanoramaEventStatsPosY.GetFloat();
			const float xPadding = 10.0f;
			const float yPadding = 5.0f;
			float x0 = xpos + xPadding;
			float y0 = ypos + yPadding;
			const float flLineOffset = 20.0f;
			const float flFontSize = 18.0f;
			const float flCol1 = x0 + 20.0f;
			float flCol2 = flCol1 + 200.0f;
			EventStatsTracker_t* pStatsTracker = &s_totalTracker;

			float flBgWidth = 630.0f;
			if( nConVar > 1 )
			{
				flBgWidth = 940.0f;
			}

			s_fmtStr.sprintf( "Panorama per-frame event stats:" );
			DrawDebugBg( pRenderEngine, xpos, ypos, flBgWidth, flLineOffset + yPadding );
			DrawDebugText( pRenderEngine, s_fmtStr.Get(), x0, y0, flFontSize );
			y0 += flLineOffset;

			DrawDebugBg( pRenderEngine, xpos, y0, flBgWidth, flLineOffset );
			DrawDebugText( pRenderEngine, "DispatchEvent calls", flCol1, y0, flFontSize );
			s_fmtStr.sprintf(
				"%d (min: %d, max: %d, avg: %d)",
				pStatsTracker->m_cur.m_nEvents,
				pStatsTracker->m_min.m_nEvents,
				pStatsTracker->m_max.m_nEvents,
				pStatsTracker->m_avg.m_nEvents );
			DrawDebugText( pRenderEngine, s_fmtStr.Get(), flCol2, y0, flFontSize );
			y0 += flLineOffset;

			DrawDebugBg( pRenderEngine, xpos, y0, flBgWidth, flLineOffset );
			DrawDebugText( pRenderEngine, "DispatchEvent timing", flCol1, y0, flFontSize );
			s_fmtStr.sprintf(
				"%4.3f ms (min: %4.3f, max: %4.3f, avg: %4.3f)",
				(float)pStatsTracker->m_cur.m_cycleCount.GetMillisecondsF(),
				(float)pStatsTracker->m_min.m_cycleCount.GetMillisecondsF(),
				(float)pStatsTracker->m_max.m_cycleCount.GetMillisecondsF(),
				(float)pStatsTracker->m_avg.m_cycleCount.GetMillisecondsF() );
			DrawDebugText( pRenderEngine, s_fmtStr.Get(), flCol2, y0, flFontSize );
			y0 += flLineOffset;

			if( nConVar > 1 )
			{
				DrawDebugBg( pRenderEngine, xpos, y0, flBgWidth, flLineOffset );
				y0 += flLineOffset;
				DrawDebugBg( pRenderEngine, xpos, y0, flBgWidth, flLineOffset );
				DrawDebugText( pRenderEngine, "Event(s) with highest max times:", x0, y0, flFontSize );
				y0 += flLineOffset;

				flCol2 += 100.0f;

				for( int i = 0; i < MIN( (nConVar - 1), s_sortedEventStats.Count() ); ++i )
				{
					PerEventStats_t* pEventStats = s_sortedEventStats.Element( i );
					pStatsTracker = &pEventStats->m_tracker;
					DrawDebugBg( pRenderEngine, xpos, y0, flBgWidth, flLineOffset );
					DrawDebugText( pRenderEngine, pEventStats->m_type.String(), flCol1, y0, flFontSize );
					s_fmtStr.sprintf(
						"avg %d ev/f (min: %d, max: %d), avg: %4.3f ms/f (min: %4.3f, max: %4.3f)",
						pStatsTracker->m_avg.m_nEvents,
						pStatsTracker->m_min.m_nEvents,
						pStatsTracker->m_max.m_nEvents,
						pStatsTracker->m_avg.m_cycleCount.GetMillisecondsF(),
						pStatsTracker->m_min.m_cycleCount.GetMillisecondsF(),
						pStatsTracker->m_max.m_cycleCount.GetMillisecondsF()
					);
					DrawDebugText( pRenderEngine, s_fmtStr.Get(), flCol2, y0, flFontSize );
					y0 += flLineOffset;
				}
			}
			DrawDebugBg( pRenderEngine, xpos, y0, flBgWidth, yPadding );

			pRenderEngine->PopAnimationAndTransformContext( 0 );
		}
	}
#else
	static void EventStatsFrameUpdate() {}
	static void EventStatsUpdate( CPanoramaSymbol eventType, CCycleCount const&eventCycleCount ) {}
	static void EventStatsDraw( CUIRenderEngine * pRenderEngine ) {}
#endif // PANORAMA_EVENT_STATS_ENABLED

#if !defined( SOURCE2_PANORAMA )
	static CCommandLineParam g_PanoramaOpenGLBackend( "-gl", "Use the SDL/OpenGL rendering pipeline" );
#endif

#ifdef DBGFLAG_VALIDATE
	void ValidateStaticsInternal( CValidator &validator )
	{
		ValidateGlobalEvents( validator );
		ValidatePanel2DFactory( validator );
		ValidateStylePropertyFactory( validator );
		CNet::ValidateGlobals( validator );
		
#if !defined( PANORAMA_DISABLE_VIDEO )
		VideoValidateStatics( validator );
#endif
		Sys_ValidateModules( &validator );
		ValidateAudio( validator );
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
		ClientAPI_Validate( validator );
#endif

#ifdef _WIN32
		CUIEngineWin32::ValidateStatics( validator );
		ValidateObj( g_CompletionPortManager );
#endif
	}
#endif

	bool CUIEngine::s_bGlobalInitDone = false;
	CThreadMutex CUIEngine::s_MutexGlobalInit;
	static v8::Platform *s_pNativeV8Platform = nullptr;
	int CUIEngine::s_nUIEnginesActive = 0;

	//-----------------------------------------------------------------------------
	// Purpose:	Central error routine for all Require* methods.
	//-----------------------------------------------------------------------------
	void RequiredCallFailed( const char *pchFormat, ... )
	{
		char chString[1000];
		va_list args;

		va_start( args, pchFormat );
		V_vsprintf_safe( chString, pchFormat, args );
		va_end( args );

		Plat_FatalError( "%s", chString );
	}

	//-----------------------------------------------------------------------------
	// Purpose:	Factory function for UI engine creation
	//-----------------------------------------------------------------------------
	PANORAMA_INTERFACE IUIEngine *CreatePanoramaUIEngineInternal()
	{
#if !defined( SOURCE2_PANORAMA ) && (!defined(WIN32) || defined(OGL))
		if ( !IsWindows() ||  CommandLine()->CheckParm( g_PanoramaOpenGLBackend.GetHParam() ) )
		{
			CUIEngineSDL *pUIEngine = new CUIEngineSDL();
			if ( !pUIEngine->BInitialize() )
			{
				delete pUIEngine;
				return NULL;
			}
			g_IUITextServices->InitializeServices();
			return pUIEngine;
		}
#ifdef WIN32
		else
		{
			CUIEngineWin32 *pUIEngine = new CUIEngineWin32();
			if ( !pUIEngine->BInitialize() )
			{
				delete pUIEngine;
				return NULL;
			}
			g_IUITextServices->InitializeServices();
			return pUIEngine;
		}
#endif
#elif defined( SOURCE2_PANORAMA )
		CUIEngineSource2 *pUIEngine = new CUIEngineSource2();
		if ( !pUIEngine->BInitialize() )
		{
			delete pUIEngine;
			return NULL;
		}
		g_IUITextServices->InitializeServices();
		return pUIEngine;

#else
		// win32 d3d10 only path
		CUIEngineWin32 *pUIEngine = new CUIEngineWin32();
		if ( !pUIEngine->BInitialize() )
		{
			delete pUIEngine;
			return NULL;
		}
		g_IUITextServices->InitializeServices();
		return pUIEngine;

#endif

	}

	//-----------------------------------------------------------------------------
	// Purpose: Helper job for async web api requests
	//-----------------------------------------------------------------------------
	class CJSAsyncWebRequest
	{
	public:
		CJSAsyncWebRequest( HTTPRequestHandle hRequest, const char *pchURL, IUIPanel * ptrPanelContext,
			v8::Persistent<v8::Function> *pSuccessFunc, v8::Persistent<v8::Function> *pFailureFunc, v8::Persistent<v8::Function> *pCompleteFunc );

		virtual ~CJSAsyncWebRequest();

		void StartRequest();

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const char *pchName );
#endif

	private:
		static void XMLHttpRequestObjectFromHTTPResponse( HTTPRequestCompleted_t *pParam, const char *pchStatus, v8::Handle<v8::Value> *pXHROut );

		STEAM_CALLRESULT( CJSAsyncWebRequest, HTTPRequestCompleted, HTTPRequestCompleted_t );

		v8::Persistent<v8::Function> *m_pSuccessFunc;
		v8::Persistent<v8::Function> *m_pFailureFunc;
		v8::Persistent<v8::Function> *m_pCompleteFunc;

		HTTPRequestHandle m_hRequest;
		CFileResource m_fileResource;
		CPanelPtr<IUIPanel> m_pPanelPtr;
	};
}

//-----------------------------------------------------------------------------
// Purpose: Context panel stack for JS. The target panel is the one on top of
// the stack, and that's what's returned by JSGetContextPanel
//-----------------------------------------------------------------------------

const int PANEL_STACK_SIZE = 64;
static panorama::IUIPanel *gPanelStack[ PANEL_STACK_SIZE ];
static int gPanelStackTop = 0;

void CheckThreadID()
{
#if (V8_CTX_DBG_SPEW_ENABLED)
	// Very basic, non thread-safe thread safety check
	static DWORD s_threadID = 0;
	DWORD threadID = GetCurrentThreadId();

	if ( s_threadID == 0 )
	{
		s_threadID = threadID;
	}
	else if ( s_threadID != threadID )
	{
		V8_CtxDbgAssert( "Thread ID mismatch" );
	}
#endif
}

void CUIEngine::PushContextPanel( panorama::IUIPanel *pPanel )
{
	CheckThreadID();

	if ( gPanelStackTop >= PANEL_STACK_SIZE )
	{
		Error( "Panorama context panel stack overflow. Increase PANEL_STACK_SIZE in uiengine.cpp" );
	}
	else
	{
		gPanelStackTop++;
		gPanelStack[ gPanelStackTop ] = pPanel;
	}
}

void CUIEngine::PopContextPanel()
{
	CheckThreadID();

	if ( gPanelStackTop <= 0 )
	{
		V8_CtxDbgAssert( "Stack underlow" );
	}
	else
	{
		gPanelStack[ gPanelStackTop ] = nullptr;
		gPanelStackTop--;
	}
}

//-----------------------------------------------------------------------------
// Purpose:	 Cancel previously created web api request job
//-----------------------------------------------------------------------------
void CUIEngine::CancelAsyncJSONWebAPIRequest( uint32 requestID )
{
	HTTPRequestHandle handle = (HTTPRequestHandle)requestID;
	int iMap = m_MapInFlightJSONHTTPRequests.Find( handle );
	if ( iMap != m_MapInFlightJSONHTTPRequests.InvalidIndex() )
		m_MapInFlightJSONHTTPRequests[iMap].m_bCanceled = true;
}



bool CUIEngine::QueuedMsgSort( const CUIEngine::QueuedEvent_t &lhs, const CUIEngine::QueuedEvent_t &rhs, void *pCtx ) { return lhs < rhs; }


v8::ArrayBuffer::Allocator *CUIEngine::s_V8ArrayBufferAllocator = nullptr;


//-----------------------------------------------------------------------------
// Purpose: CPanoramaSymbol support for cross DLL symbols
//-----------------------------------------------------------------------------
namespace panorama
{
UtlSymId_t MakeSymbol( const char *pchText )
{
	return CUtlSymbol( pchText );
}


//-----------------------------------------------------------------------------
// Purpose: CPanoramaSymbol support for cross DLL symbols
//-----------------------------------------------------------------------------
const char * ResolveSymbol( const UtlSymId_t sym )
{
	return CUtlSymbol( sym ).String();
}
}


//-----------------------------------------------------------------------------
// Purpose: CPanoramaSymbol support for cross DLL symbols
//-----------------------------------------------------------------------------
UtlSymId_t CUIEngine::MakeSymbol( const char *pchText )
{
	return panorama::MakeSymbol( pchText );
}


//-----------------------------------------------------------------------------
// Purpose: CPanoramaSymbol support for cross DLL symbols
//-----------------------------------------------------------------------------
const char * CUIEngine::ResolveSymbol( const UtlSymId_t sym )
{
	return panorama::ResolveSymbol( sym );
}


//-----------------------------------------------------------------------------
// Purpose: Fatal V8 error handler
//-----------------------------------------------------------------------------
void V8FatalErrorHandler( const char* location, const char* message )
{
	AssertFatalMsg2( false, "Fatal V8 Error: %s - %s", location, message );
}

// Static serial number, want to continue to grow across CUIEngine restarts so that 
// CPanelPtr values can never be re-used and valid on accident if we restart
CInterlockedUInt CUIEngine::m_unPanelSerialNumber = 0;


//-----------------------------------------------------------------------------
// Purpose: compares the two scheduled items
//-----------------------------------------------------------------------------
bool CUIEngine::ScheduledItemSortFunc( ScheduledItem_t const &lhs, ScheduledItem_t const &rhs )
{
	return (lhs.m_flFrameTime > rhs.m_flFrameTime);
}

class CNoOpFileSystem : public IUIFileSystem
{
public:
	// fully load the file into the buffer object
	virtual bool LoadFileIntoBuffer( const char *pchFile, CUtlBuffer &buf, bool bText, FileChangeCallback_t fileChangeCallback = NULL, uint nPadding = 0 )  { return false; }

	// fully load the file into the buffer object, async variant
	virtual HLOADINTOBUFFER LoadFileIntoBufferAsync( const char *pchFile, CUtlBuffer &buf, bool bText, CUtlDelegate< LoadFileIntoBufferCallback_t > del ) { del( pchFile, buf, false ); return 0; }

	// Cancel async load
	virtual bool CancelLoadFileIntoBufferAsync( HLOADINTOBUFFER hLoad ) { return false; }

	// replace this file with the contents of the buffer object
	virtual bool SaveBufferToFile( CUtlBuffer &buf, const char *pchFile ) { return false; }

	// return true if this file is on disk (or in a resource file)
	virtual bool FileExists( const char *pchFile )  { return false; }

	// Run frame on main thread
	virtual void RunFrame() { }

	virtual char* LoadFromPanZip( const char* fname ) { return NULL; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) {}
#endif

};
CNoOpFileSystem s_NoOpFilesystem;

#if !defined( SOURCE2_PANORAMA )
class CSteamPanoramaFileSystem;
class CSteamAsyncIoThread : public CThread
{
public:
	CSteamAsyncIoThread( CSteamPanoramaFileSystem *pFileSystem )
	{
		m_pFileSystem = pFileSystem;
		m_bExit = false;
	}

	~CSteamAsyncIoThread()
	{

	}

	void Stop() { m_bExit = true; }

	virtual int Run() OVERRIDE;
	

private:
	volatile bool m_bExit;
	CSteamPanoramaFileSystem *m_pFileSystem;
};


class CSteamPanoramaFileSystem : public IUIFileSystem
{
public:

	CSteamPanoramaFileSystem() : m_Thread( this )
	{
		m_Thread.SetName( "Panorama AsyncIO" );
		m_Thread.Start();
	}

	~CSteamPanoramaFileSystem()
	{
		m_Thread.Stop();
		m_Thread.Join();

		FOR_EACH_LL( m_llAsyncIORequests, i )
		{
			delete m_llAsyncIORequests.Element( i );
		}
		m_llAsyncIORequests.RemoveAll();

		FOR_EACH_LL( m_llAsyncIOResults, i )
		{
			delete m_llAsyncIOResults[i];
		}
		m_llAsyncIOResults.RemoveAll();
	}

	// fully load the file into the buffer object
	virtual bool LoadFileIntoBuffer( const char *pchFile, CUtlBuffer &buf, bool bText, FileChangeCallback_t fileChangeCallback = NULL )  { return ::LoadFileIntoBuffer( pchFile, buf, bText ); }

	// fully load the file into the buffer object, async variant that does threaded io 
	virtual HLOADINTOBUFFER LoadFileIntoBufferAsync( const char *pchFile, CUtlBuffer &buf, bool bText, CUtlDelegate< LoadFileIntoBufferCallback_t > del )
	{ 
		CLoadFileIntoBufferOperation *pOperation = new CLoadFileIntoBufferOperation( pchFile, &buf, bText, del );

		AUTO_LOCK( m_AsyncIoLock );
		m_mapAsyncFileLoadsPending.Insert( pOperation, pOperation );
		m_llAsyncIORequests.AddToTail( pOperation );
		m_ThreadEvent.Set();

		return pOperation;
	}

	virtual bool CancelLoadFileIntoBufferAsync( HLOADINTOBUFFER hLoad )
	{
		AUTO_LOCK( m_AsyncIoLock );
		int iMap = m_mapAsyncFileLoadsPending.Find( hLoad );
		if ( iMap != m_mapAsyncFileLoadsPending.InvalidIndex() )
		{
			CAsyncIOOperation *pOperation = m_mapAsyncFileLoadsPending[iMap];
			pOperation->SetCancelled();
			return true;
		}

		return false;
	}


	// replace this file with the contents of the buffer object
	virtual bool SaveBufferToFile( CUtlBuffer &buf, const char *pchFile ) { return ::SaveBufferToFile( buf, pchFile );  }

	// return true if this file is on disk (or in a resource file)
	virtual bool FileExists( const char *pchFile )  { return ::BFileExists( pchFile ); }


	// Run frame on main thread
	virtual void RunFrame()
	{
		VPROF_BUDGET( "CSteamPanoramaFileSystem::RunFrame", VPROF_BUDGETGROUP_TENFOOT );
		m_AsyncIoLock.Lock();
		while( m_llAsyncIOResults.Count() )
		{
			int iHead = m_llAsyncIOResults.Head();
			CAsyncIOResult *pResult = m_llAsyncIOResults.Element( iHead );
			m_llAsyncIOResults.Remove( iHead );

			m_AsyncIoLock.Unlock();

			if ( !pResult->BIsCancelled() )
				pResult->DispatchResult();

			m_AsyncIoLock.Lock();

			// Delete from map of pending loads
			m_mapAsyncFileLoadsPending.Remove( pResult->GetOperation() );

			delete pResult;
		}
		m_AsyncIoLock.Unlock();
	}


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		AUTO_LOCK( m_AsyncIoLock );
		ValidateObj( m_llAsyncIORequests );
		FOR_EACH_LL( m_llAsyncIORequests, i )
		{
			ValidatePtr( m_llAsyncIORequests[i] );
		}
		ValidateObj( m_llAsyncIOResults );
		FOR_EACH_LL( m_llAsyncIOResults, i )
		{
			ValidatePtr( m_llAsyncIOResults[i] );
		}
	}
	#endif
private:

	friend class CSteamAsyncIoThread;

	class CAsyncIOOperation;

	class CAsyncIOResult
	{
	public:
		CAsyncIOResult() { m_pOperation = NULL; }
		void SetOperation( CAsyncIOOperation *pOperation ) { m_pOperation = pOperation; }
		CAsyncIOOperation *GetOperation() { return m_pOperation; }
		bool BIsCancelled() { return m_pOperation->BIsCancelled(); }
		virtual ~CAsyncIOResult() 
		{
			delete m_pOperation;
		}
		virtual void DispatchResult() = 0;


#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName ) {}
#endif

	private:
		CAsyncIOOperation *m_pOperation;
	};

	class CAsyncIOOperation
	{
	public:
		CAsyncIOOperation()
		{
			m_bCancelled = false;
		}
		
		virtual ~CAsyncIOOperation() 
		{
		}

		virtual CAsyncIOResult * RunOnThread() = 0;
		
		bool BIsCancelled() 
		{
			return m_bCancelled;
		}

		void SetCancelled()
		{
			m_bCancelled = true;
		}

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName ) {}
#endif
	private:
		bool m_bCancelled;
	};
	
	class CLoadFileIntoBufferOperation : public CAsyncIOOperation
	{
	public:
		CLoadFileIntoBufferOperation( const char *pchFile, CUtlBuffer *pBufOut, bool bText, CUtlDelegate< LoadFileIntoBufferCallback_t > callback )
		{
			m_strFile = pchFile;
			m_pBuffer = pBufOut;
			m_bText = bText;
			m_Del = callback;
		}

		virtual CAsyncIOResult * RunOnThread() OVERRIDE
		{
			bool bResult = ::LoadFileIntoBuffer( m_strFile.String(), *(m_pBuffer), m_bText );
			return new CLoadFileIntoBufferOperationResult( m_strFile.String(), m_pBuffer, bResult, m_Del );
		}

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName ) 
		{
			CAsyncIOOperation::Validate( validator, pchName );
			ValidateObj( m_strFile );
			ValidatePtr( m_pBuffer );
		}
#endif
	private:
		CUtlString m_strFile;
		CUtlBuffer *m_pBuffer;
		bool m_bText;
		CUtlDelegate< LoadFileIntoBufferCallback_t > m_Del;
	};

	class CLoadFileIntoBufferOperationResult : public CAsyncIOResult
	{
	public:
		CLoadFileIntoBufferOperationResult( const char *pchFile, CUtlBuffer *pBufOut, bool bSuccess, CUtlDelegate< LoadFileIntoBufferCallback_t > callback )
		{
			m_strFile = pchFile;
			m_pBuffer = pBufOut;
			m_bSuccess = bSuccess;
			m_Del = callback;
		}

		virtual void DispatchResult()
		{
			m_Del( m_strFile.String(), *m_pBuffer, m_bSuccess );
		}

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{
			CAsyncIOResult::Validate( validator, pchName );
			ValidateObj( m_strFile );
			ValidatePtr( m_pBuffer );
		}
#endif
	private:
		CUtlString m_strFile;
		CUtlBuffer *m_pBuffer;
		bool m_bSuccess;
		CUtlDelegate< LoadFileIntoBufferCallback_t > m_Del;
	};
	

	CThreadMutex m_AsyncIoLock;
	CThreadEvent m_ThreadEvent;
	CUtlLinkedList< CAsyncIOOperation *, int > m_llAsyncIORequests;
	CUtlLinkedList< CAsyncIOResult *, int > m_llAsyncIOResults;
	CUtlMap< HLOADINTOBUFFER, CAsyncIOOperation *, int, CDefLess< HLOADINTOBUFFER > > m_mapAsyncFileLoadsPending;
	CSteamAsyncIoThread m_Thread;

};

int CSteamAsyncIoThread::Run()
{
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
	CVProfile *pProfile = GetVProfProfileForCurrentThread();
#endif

	while ( !m_bExit )
	{
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
		if ( pProfile )
			pProfile->MarkFrame( "UIEngine Main Thread" );
#endif
		{
			VPROF_BUDGET( "CSteamAsyncIoThread - run", VPROF_BUDGETGROUP_TENFOOT );
			m_pFileSystem->m_AsyncIoLock.Lock();
			while ( m_pFileSystem->m_llAsyncIORequests.Count() )
			{
				CSteamPanoramaFileSystem::CAsyncIOOperation *pReq = m_pFileSystem->m_llAsyncIORequests.Element( m_pFileSystem->m_llAsyncIORequests.Head() );
				m_pFileSystem->m_llAsyncIORequests.Remove( m_pFileSystem->m_llAsyncIORequests.Head() );

				bool bCancelled = pReq->BIsCancelled();

				m_pFileSystem->m_AsyncIoLock.Unlock();

				CSteamPanoramaFileSystem::CAsyncIOResult * pResult = NULL;

				if ( !bCancelled )
					pResult = pReq->RunOnThread();
				
				m_pFileSystem->m_AsyncIoLock.Lock();

				// Check again since we took lock once more and may have canceled while re ran thread work
				bCancelled = pReq->BIsCancelled();

				if ( !bCancelled && pResult )
				{
					pResult->SetOperation( pReq );
					m_pFileSystem->m_llAsyncIOResults.AddToTail( pResult );
				}
				else 
				{
					if ( pResult )
						delete pResult;
					
					// Delete from map of pending loads
					m_pFileSystem->m_mapAsyncFileLoadsPending.Remove( pReq );
					delete pReq;
				}

				if ( m_bExit )
					break;
			}
			m_pFileSystem->m_AsyncIoLock.Unlock();
		}

		if ( !m_bExit )
		{
			VPROF_BUDGET( "CSteamAsyncIoThread - sleep", VPROF_BUDGETGROUP_TENFOOT );
			m_pFileSystem->m_ThreadEvent.Wait( 2000 );
		}
	}

	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIEngine::CUIEngine()  : 
#if !defined( SOURCE2_PANORAMA )
	m_vecQueuedEvents( QueuedMsgSort ),
	m_PanelStylePool( sizeof( CPanelStyle ), 256 ),
#else
	m_PanelStylePool( sizeof( CPanelStyle ), 256, VALIGNOF( CPanelStyle ) ),
#endif
	m_QueueScheduledDelegates( 0, 0, &CUIEngine::ScheduledItemSortFunc ),
	m_HTTPRequestCompleted( this, &CUIEngine::OnHTTPJSONWebAPIRequestFinished )
{
#if !defined( SOURCE2_PANORAMA )
	m_pHTMLController = NULL;
	m_pFileSystem = new CSteamPanoramaFileSystem();
#else
	m_pFileSystem = &s_NoOpFilesystem;
#endif

#if !defined( SOURCE2_PANORAMA )
	if( CommandLine()->CheckParm( g_DevMode.GetHParam() ) || Plat_IsInDebugSession() )
		g_cvarDeveloper.SetValue( 1 );
#endif

	// Stuff we init once per process...
	{
		AUTO_LOCK( s_MutexGlobalInit );
		if( !s_bGlobalInitDone )
		{
			// This stuff we do once per process period.
			s_bGlobalInitDone = true;

			// early startup stuff so we can load either 10' or 2' startup UIs
			DeclareCurrentThreadIsMainThread();

			int nV8ThreadPoolSize = 0;
			
#if defined( SOURCE2_PANORAMA )
			// Reduce the amount of stack that V8 can use so that we
			// can keep our thread stack sizes small.
			const char *pV8InitFlags = "--stack-size=384";
			v8::V8::SetFlagsFromString( pV8InitFlags, V_strlen( pV8InitFlags ) );

			// source2 doesn't need heavy JavaScript threading so
			// restrict v8 to just one thread.
			nV8ThreadPoolSize = CommandLine()->ParmValue( "-pv8poolthreads", 1 );
#endif
			
			// bugbug jmccaskey - do these things once per process, not per UI engine, or even once per panorama.dll load!
			v8::Platform* platform = v8::platform::NewDefaultPlatform( nV8ThreadPoolSize ).release();
			s_pNativeV8Platform = platform;
			v8::V8::InitializeICUDefaultLocation( nullptr );
			v8::V8::InitializePlatform( platform );
			v8::V8::Initialize();

			//v8::V8::SetFlagsFromCommandLine( &argc, argv, true );

			const char* v8version = v8::V8::GetVersion();
			Msg("V8 Version: %s\n", v8version);

			PushContextPanel( nullptr );
		}

		++s_nUIEnginesActive;

		// This stuff we do if we are the only engine at our init time, this could be done multiple times per process 
		// if we are shutdown then recreated.
		if( s_nUIEnginesActive == 1 )
		{
			// bugbug jmccaskey - make this a SteamAPI only dependancie?  Ok for ClientController to be SteamController,
			// but ClientVR is the big issue right now.  We'll kill that game side.
#if !defined( SOURCE2_PANORAMA )
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
			bool bSuccess = ClientAPI_Init();
			AssertMsg( bSuccess, "ClientAPI_Init failed" );
#else
			bool bSuccess = SteamAPI_Init();
			REFERENCE( bSuccess );
#endif
#endif
		}
	}



	Assert( g_IUIEngine == NULL );
	g_IUIEngine = this;
	m_bAggressivelyLimitFrameRate = false;
	m_bAggressivelyLimitWindowFPS = false;
	m_flLastV8IncrementalGC = 0.0f;
	m_bPaintCountTrackingEnabled = false;
	m_bShutdown = false;
	m_bShuttingdown = false;
	m_bInited = false;
	m_bWorkRemaining = false;
	m_pPanelZooWindow = NULL;
	m_pConsoleWindow = NULL;
	m_pUILayoutManager = NULL;
	m_pLocalization = NULL;
	m_pSoundSystem = NULL;
	m_pInputEngine = new CUIInputEngine();
	m_bPaintedWindows = false;
	m_flLastInputTime = 0.0f;
	m_flLastUserActiveReportTime = 0.0f;
	m_pSettings = NULL;
	m_pLastDispatchedEventTargetPanel = NULL;
	m_bDebuggerActive = false;
	m_unNextScheduledJSHandle = 0;

	m_ulFramesTimeWentBackward = 0;
	m_flCurrentFrameTime = Plat_FloatTime();
	m_flLastScheduledDelRunTime = 0.0f;

	m_unMaxPanelPaintsSinceReset = 0;

	m_pStyleFactory = new CStyleFactoryWrapper();
	m_unNextEventHandlerId = 0;
	m_nNextGenericCallbackHandle = 0;

	m_nCurRegisterJSScope = -1;

	v8::Isolate::CreateParams createParams;
	
#if defined( SOURCE2_PANORAMA )
	
	// Use the native V8 provider's allocator; its base is built without RTTI.
	if ( !s_V8ArrayBufferAllocator )
		s_V8ArrayBufferAllocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	createParams.array_buffer_allocator = s_V8ArrayBufferAllocator;
	v8::ResourceConstraints &resConstraints = createParams.constraints;

	// Pretend that we're a medium-memory machine to keep v8's
	// memory limits small.  We could explicitly set them even
	// smaller but it's unclear where they should be set.
#ifdef PLATFORM_64BITS
	// Let memory reservation go higher on 64-bit as we
	// aren't address-space-constrained and it can help
	// with tools-mode / development.
	resConstraints.ConfigureDefaults( 1024 * 1024 * 1024, 0  );
#else
	resConstraints.ConfigureDefaults( 768 * 1024 * 1024, 0 );
#endif

	// Allow experimentation with some of the constraints.
	// Memory values are all in megabytes.  These usually
	// need to be a power of two and at least four.
	if ( CommandLine()->HasParm( "-pv8semispace" ) )
	{
		resConstraints.set_max_young_generation_size_in_bytes( size_t( CommandLine()->ParmValue( "-pv8semispace", 1 ) ) * 3 * 1024 * 1024 );
	}
	if ( CommandLine()->HasParm( "-pv8oldspace" ) )
	{
		resConstraints.set_max_old_generation_size_in_bytes( size_t( CommandLine()->ParmValue( "-pv8oldspace", 1 ) ) * 1024 * 1024 );
	}
	if ( CommandLine()->HasParm( "-pv8execspace" ) )
	{
		Warning( "V8 9.4 does not expose a maximum executable heap size; -pv8execspace is unsupported.\n" );
	}
	
#endif

	m_pV8Isolate = v8::Isolate::New( createParams );
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	m_pV8Isolate->SetFatalErrorHandler( &V8FatalErrorHandler );
	m_bDoV8GarbageCollect = false;

	v8::HandleScope handle_scope( m_pV8Isolate );

#if V8_DEBUGGING_ENABLED
	if ( panorama_remote_debug.GetBool() )
	{
		EnableRemoteDebugger();

		// Pause indefinitely waiting for debugger front end to connect
		uint32_t startTime = Plat_MSTime();

		if ( CommandLine()->CheckParm( "-panorama_wait_debugger") )
		{
			while ( !m_pWebsocketServer->IsConnected() )
			{
				m_pWebsocketServer->RunFrame();
				ThreadSleep( 100 );

				uint32_t time = Plat_MSTime();
				if ( (time - startTime) > 1000 )
				{
					Msg( "*** Waiting for debugger front end, because of cmd line parameter -panorama_wait_debugger.\n" );
					startTime = time;
				}
			}
			
			// Finish handshake
			m_pWebsocketServer->RunFrame( 0, true );		
		}
	}
#endif

	InitializePanoramaContext( &m_V8UIEngineGlobalContext );
	SetContextSecurityToken( m_pV8Isolate, *(m_V8UIEngineGlobalContext.Get( m_pV8Isolate )), GLOBAL_CONTEXT_SEC_TOKEN );

	// Setup our event registration map, and register all the framework internal events immediately
	RegisterEventTypesWithEngine( this );
	RegisterPanelFactoriesWithEngine( this );

#ifdef PANORAMA_EVENT_STATS_ENABLED
	s_totalFrameStats.InitZero();
	s_totalTracker.InitZero();
	s_perEventStatsMap.Purge();
	s_perEventStats.Purge();
	s_sortedEventStats.Purge();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIEngine::~CUIEngine()
{
	if ( !m_bShutdown )
		Shutdown();

#if V8_DEBUGGING_ENABLED
	DisableRemoteDebugger();
#endif

	RunQueuedDecRefCalls();

#if !defined( SOURCE2_PANORAMA )
	delete (CSteamPanoramaFileSystem*)(m_pFileSystem);
	m_pFileSystem = NULL;
#endif

	Assert( g_IUIEngine == this );
	g_IUIEngine = NULL;

	FOR_EACH_RBTREE_FAST( m_treeInFlightJSAsyncWebequestObjects, i )
	{
		delete m_treeInFlightJSAsyncWebequestObjects[i];
	}
	m_treeInFlightJSAsyncWebequestObjects.RemoveAll();

	FOR_EACH_HASHMAP( m_mapPanelTypeEventHandlers, i )
	{
		delete m_mapPanelTypeEventHandlers[i];
	}
	m_mapPanelTypeEventHandlers.RemoveAll();

	FOR_EACH_VEC( m_vecV8GlobalObjectRegistrations, i )
	{
		m_vecV8GlobalObjectRegistrations[i].m_pObj->Reset();
		delete m_vecV8GlobalObjectRegistrations[i].m_pObj;
	}
	m_vecV8GlobalObjectRegistrations.Purge();

	FOR_EACH_VEC( m_vecV8GlobalFunctionRegistrations, i )
	{
		m_vecV8GlobalFunctionRegistrations[i].m_pFunction->Reset();
		delete m_vecV8GlobalFunctionRegistrations[i].m_pFunction;
	}
	m_vecV8GlobalObjectRegistrations.Purge();

	FOR_EACH_MAP( m_MapV8GlobalObjectInstances, i )
	{
		m_MapV8GlobalObjectInstances[i]->Reset();
		delete m_MapV8GlobalObjectInstances[i];
	}

	m_V8PanelStyleTemplate.Reset();

	// Done with UIEngine global context now
	m_V8GlobalTemplate.Reset();
	FOR_EACH_MAP_FAST( m_mapV8PanelClassTemplates, i )
	{
		m_mapV8PanelClassTemplates.Element( i )->Reset();
	}
	m_mapV8PanelClassTemplates.PurgeAndDeleteElements();


	FOR_EACH_MAP_FAST( m_mapV8ClassTemplatesByType, i )
	{
		m_mapV8ClassTemplatesByType.Element( i )->Reset();
	}
	m_mapV8ClassTemplatesByType.PurgeAndDeleteElements();

	m_V8UIEngineGlobalContext.Reset();

	m_pV8Isolate->Dispose();
	m_pV8Isolate = NULL;

	{
		AUTO_LOCK( s_MutexGlobalInit );
		--s_nUIEnginesActive;

		if( s_nUIEnginesActive == 0 )
		{
#if !defined( SOURCE2_PANORAMA )
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
			ClientAPI_Shutdown();
#else
			SteamAPI_Shutdown();
#endif
#endif
		}
	}

	// IMPORTANT - Don't do the below ever.  Can not re-initialize until process restart! 
	// We just let V8 continue to run until process exit if we've ever created a UIEngine,
	// there is not much overhead from doing this rather than shutting it down.
	/*
	{
		AUTO_LOCK( s_MutexJSInit );
		if( s_bJSIsInitialized )
		{
			v8::V8::Dispose();
			s_bJSIsInitialized = false;
		}
	}*/


	SAFE_DELETE( m_pStyleFactory );
}


//-----------------------------------------------------------------------------
// Purpose: Hookup convars
//-----------------------------------------------------------------------------
void CUIEngine::ConCommandInit( IConCommandBaseAccessor *pAccessor )
{
#if defined( SOURCE2_PANORAMA )
	ConVar_Register( 0, pAccessor );
#else
	ConCommandBaseMgr::OneTimeInit( pAccessor );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Create a text layout object and return
//-----------------------------------------------------------------------------
IUITextLayout *CUIEngine::CreateTextLayout( const char *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight )
{
	IUITextServices::TextLayoutParams_t params( GetDisplayLanguage(), pchFontName, flSize, flLineHeight, flMaxWidth, flMaxHeight );
	params.m_weight = weight;
	params.m_style = style;
	params.m_align = align;
	params.m_bWrap = bWrap;
	params.m_bEllipsis = bEllipsis;
	params.m_nLetterSpacing = nLetterSpacing;
	int cTextChars = V_UnicodeLength( pchText );
	int cTextBytes = V_strlen( pchText ) + 1;
	return UITextServices()->CreateTextLayout( pchText, cTextBytes, cTextChars, k_EPanoramaTextEncodingUTF8, &params );
}


//-----------------------------------------------------------------------------
// Purpose: Create a text layout object and return
//-----------------------------------------------------------------------------
IUITextLayout *CUIEngine::CreateTextLayout( const uchar16 *pch16Text, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight )
{
	IUITextServices::TextLayoutParams_t params( GetDisplayLanguage(), pchFontName, flSize, flLineHeight, flMaxWidth, flMaxHeight );
	params.m_weight = weight;
	params.m_style = style;
	params.m_align = align;
	params.m_bWrap = bWrap;
	params.m_bEllipsis = bEllipsis;
	params.m_nLetterSpacing = nLetterSpacing;
	int cTextChars = V_UnicodeLength( pch16Text );
	int cTextBytes = ( V_strlen16( pch16Text ) + 1 ) * sizeof( *pch16Text );
	return UITextServices()->CreateTextLayout( pch16Text, cTextBytes, cTextChars, k_EPanoramaTextEncodingUChar16, &params );
}


//-----------------------------------------------------------------------------
// Purpose: Create a text layout object and return
//-----------------------------------------------------------------------------
IUITextLayout *CUIEngine::CreateTextLayout( const uchar32 *pch32Text, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight )
{
	IUITextServices::TextLayoutParams_t params( GetDisplayLanguage(), pchFontName, flSize, flLineHeight, flMaxWidth, flMaxHeight );
	params.m_weight = weight;
	params.m_style = style;
	params.m_align = align;
	params.m_bWrap = bWrap;
	params.m_bEllipsis = bEllipsis;
	params.m_nLetterSpacing = nLetterSpacing;
	int cTextChars = V_UnicodeLength( pch32Text );
	int cTextBytes = ( V_strlen32( pch32Text ) + 1 ) * sizeof( *pch32Text );
	return UITextServices()->CreateTextLayout( pch32Text, cTextBytes, cTextChars, k_EPanoramaTextEncodingUChar32, &params );
}


//-----------------------------------------------------------------------------
// Purpose: Free a text layout object
//-----------------------------------------------------------------------------
void CUIEngine::FreeTextLayout( IUITextLayout *pLayout )
{
	return UITextServices()->FreeTextLayout( pLayout );
}


//-----------------------------------------------------------------------------
// Purpose: Return the set of valid font names
//-----------------------------------------------------------------------------
const CUtlSortVector< CUtlString > &CUIEngine::GetSortedValidFontNames()
{
	return UITextServices()->GetSortedValidFontNames();
}


//-----------------------------------------------------------------------------
// Purpose: Register any delegate to run at specified time, be sure to use 
// CancelScheduledDelgate if you delete the object the delgate runs on, etc.
//-----------------------------------------------------------------------------
int CUIEngine::RegisterScheduledDelegate( double flTargetFrameTime, CUtlDelegate< void() > del, const char *pchName )
{
	VPROF_BUDGET( "CUIEngine::RegisterScheduledDelegate", VPROF_BUDGETGROUP_TENFOOT );
	Assert( flTargetFrameTime > m_flCurrentFrameTime );

	int index = m_ListScheduledDelegates.AddToTail( del );

	ScheduledItem_t item;
	item.m_flFrameTime = flTargetFrameTime;
	item.m_iListIndex = index;
#if !defined( SOURCE2_PANORAMA )
	item.m_sName = pchName;
#endif

	m_QueueScheduledDelegates.Insert( item );

	return index;
}


//-----------------------------------------------------------------------------
// Purpose: Cancel a scheduled delegate by index returned from RegisterScheduledDelegate
//-----------------------------------------------------------------------------
void CUIEngine::CancelScheduledDelegate( int iScheduleIndex )
{
	if( m_ListScheduledDelegates.IsValidIndex( iScheduleIndex ) )
		m_ListScheduledDelegates[iScheduleIndex].Clear();
	else
		AssertMsg( false, "Invalid index to CancelScheduledDelegate" );
}

//-----------------------------------------------------------------------------
// Purpose: Used internally by initialization code to register panels with framework
//-----------------------------------------------------------------------------
void CUIEngine::RegisterPanelFactoryWithEngine( CPanoramaSymbol symPanelType, CPanel2DFactory *pFactory )
{
	m_mapPanelRegistrations.InsertOrReplace( symPanelType, pFactory );
}


//-----------------------------------------------------------------------------
// Purpose: Is the panel type registered
//-----------------------------------------------------------------------------
bool CUIEngine::BRegisteredPanelType( CPanoramaSymbol symPanelType )
{
	return m_mapPanelRegistrations.Find( symPanelType ) != m_mapPanelRegistrations.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Purpose: Factory func for creating panels
//-----------------------------------------------------------------------------
IUIPanelClient *CUIEngine::CreatePanelClient( CPanoramaSymbol symName, const char *pchID, panorama::IUIPanel *parent )
{
	int iMap = m_mapPanelRegistrations.Find( symName );
	if( iMap == m_mapPanelRegistrations.InvalidIndex() )
		return NULL;

	CPanel2DFactory *pFactory = m_mapPanelRegistrations.Element( iMap );
	return pFactory->CreatePanelInternal( pchID, parent );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to find next base class symbol given symbol for child panel class
//-----------------------------------------------------------------------------
CPanoramaSymbol CUIEngine::GetPanelBaseClassSymbol( CPanoramaSymbol symPanelClass )
{
	int iMap = m_mapPanelRegistrations.Find( symPanelClass );
	if( iMap == m_mapPanelRegistrations.InvalidIndex() )
		return CPanoramaSymbol();

	CPanel2DFactory *pFactory = m_mapPanelRegistrations.Element( iMap );
	CPanoramaSymbol symBase = pFactory->BaseClassSymbol();
	if( symBase == symPanelClass )
		return CPanoramaSymbol();

	return symBase;
}


//-----------------------------------------------------------------------------
// Purpose: Used internally by initialization code to register events with framework
//-----------------------------------------------------------------------------
void CUIEngine::RegisterEventWithEngine( CPanoramaSymbol symEvent, UIEventFactory factory )
{
	m_mapEventRegistrations.InsertOrReplace( symEvent, factory );
}


//-----------------------------------------------------------------------------
// Purpose: Check if a event name is valid
//-----------------------------------------------------------------------------
bool CUIEngine::IsValidEventName( const CPanoramaSymbol symEvent )
{
	int iMap = m_mapEventRegistrations.Find( symEvent );
	if( iMap == m_mapEventRegistrations.InvalidIndex() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check if a event name is valid and represents a panel event
//-----------------------------------------------------------------------------
bool CUIEngine::IsValidPanelEvent( const CPanoramaSymbol symEvent, int *pParams )
{
	if( pParams )
		*pParams = 0;

	int iMap = m_mapEventRegistrations.Find( symEvent );
	if( iMap == m_mapEventRegistrations.InvalidIndex() )
		return false;

	if( pParams )
		*pParams = m_mapEventRegistrations[iMap].m_cParams;

	return m_mapEventRegistrations[iMap].m_bPanelEvent;
}


//-----------------------------------------------------------------------------
// Purpose: Creates an event from string representation
//-----------------------------------------------------------------------------
IUIEvent *CUIEngine::CreateInputEventFromSymbol( CPanoramaSymbol symEvent, IUIPanel *pPanel, EPanelEventSource_t eSource, int nRepeats )
{
	// find parse function
	int iMap = m_mapEventRegistrations.Find( symEvent );
	if( iMap == m_mapEventRegistrations.InvalidIndex() )
		return NULL;

	UIEventFactory &factory = m_mapEventRegistrations.Element( iMap );

	switch ( factory.m_eMakeUIEventType )
	{
		case k_eMakeUIEventType_NoArguments:
			return ( factory.m_pfnMakeUIEvent0 )( pPanel ? pPanel->ClientPtr() : NULL );

		case k_eMakeUIEventType_Repeats:
			return ( factory.m_pfnMakeUIEvent1Repeats )( pPanel ? pPanel->ClientPtr() : NULL, nRepeats );

		case k_eMakeUIEventType_Source:
			return ( factory.m_pfnMakeUIEvent1Source )( pPanel ? pPanel->ClientPtr() : NULL, eSource );
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Helper for javascript event definition
//-----------------------------------------------------------------------------
void JSDefineEventHelper( const v8::FunctionCallbackInfo<v8::Value>& args, bool bPanelEvent )
{
	const char *pchFuncCall = bPanelEvent ? "DefinePanelEvent" : "DefineEvent";
	
	if ( args.Length() < 2 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<512>( "%s requires a least 2 params (eventName, numArguments, [optional] argNames, [optional] description)", pchFuncCall ).String() ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value event_name(v8::Isolate::GetCurrent(),  args[ 0 ] );
	const char * pchEventName = *event_name;
	if ( !pchEventName )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<512>( "Invalid event name to %s", pchFuncCall ).String() ).ToLocalChecked() );
		return;
	}

	if ( UIEngineInternal()->IsValidEventName( pchEventName ) )
	{
		Warning( "**** Event \'%s\' already registered with panorama. Ignoring definition!\n", pchEventName );

		// gurjeets - don't throw exception here. This is bad for reload via f7/f8, as currently there is no 
		// way for scripts to undefine events, and hence no way to avoid this exception
		// args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<512>( "Event '%s' already registered with panorama. Ignoring definition!", pchEventName ).String() ).ToLocalChecked() );
		// return;
	}

	v8::Handle<v8::Number> numEventArgs = args[1]->ToNumber( GetV8Isolate()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Number>() );
	if ( numEventArgs.IsEmpty() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<512>( "Second argument to %s must be a number: number of arguments to the event", pchFuncCall ).String() ).ToLocalChecked() );
		return;
	}

	// Optional documentation arguments
	char *pchDocumentationArgs = nullptr;
	if ( args.Length() > 2 )
	{
		v8::String::Utf8Value docArgs(v8::Isolate::GetCurrent(),  args[2] );
		pchDocumentationArgs = new char[1 + docArgs.length()];
		V_strcpy( pchDocumentationArgs, *docArgs );
	}

	// Optional description
	char *pchDocumentationDescription = nullptr;
	if ( args.Length() > 3 )
	{
		v8::String::Utf8Value docDescription(v8::Isolate::GetCurrent(),  args[3] );
		pchDocumentationDescription = new char[1 + docDescription.length()];
		V_strcpy( pchDocumentationDescription, *docDescription );
	}

	// Create event factory
	UIEventFactory eventFactory;
	eventFactory.m_cParams = numEventArgs->Int32Value( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( 0 );		// TODO Handle events with params
	eventFactory.m_bPanelEvent = bPanelEvent;
	eventFactory.m_eMakeUIEventType = k_eMakeUIEventType_NoArguments;
	eventFactory.m_pfnMakeUIEvent0 = nullptr;
	eventFactory.m_pfnParseUIEvent = nullptr;
	eventFactory.m_pfnParseUIEventJS = bPanelEvent ? panorama::UIEvent::JSCreatePanelEventFromString : panorama::UIEvent::JSCreateEventFromString;
	eventFactory.m_pfnFormatUIEventArgs = panorama::UIEvent::JSFormatEventArgs;
	eventFactory.m_pchDocumentationArgs = pchDocumentationArgs;
	eventFactory.m_pchDocumentationDescription = pchDocumentationDescription;
	eventFactory.m_eDocFlags = k_eEventDocFlagNone;

	// Register event with panorama
	CPanoramaSymbol symEvent( pchEventName );
	UIEngine()->RegisterEventWithEngine( symEvent, eventFactory );
}

//-----------------------------------------------------------------------------
// Purpose: Handler for javascript DefineEvent calls
//-----------------------------------------------------------------------------
void JSDefineEvent( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	JSDefineEventHelper( args, false );
}

//-----------------------------------------------------------------------------
// Purpose: Handler for javascript DefinePanelEvent calls
//-----------------------------------------------------------------------------
void JSDefinePanelEvent( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	JSDefineEventHelper( args, true );
}

//-----------------------------------------------------------------------------
// Purpose: Helper for javascript event dispatch
//-----------------------------------------------------------------------------
void JSDispatchEventHelper( const v8::FunctionCallbackInfo<v8::Value>& args, bool bAsyncDispatch, CUtlMap< CPanoramaSymbol, UIEventFactory, int, CDefLess< CPanoramaSymbol > > &mapEvents )
{
	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );

	const char *pchFuncCall = bAsyncDispatch ? "DispatchEventAsync" : "DispatchEvent";

	if( args.Length() < (bAsyncDispatch ? 2 : 1) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<512>( "Not enough arguments to %s", pchFuncCall ).String() ).ToLocalChecked() );
		return;
	}

	int iNameArg = bAsyncDispatch ? 1 : 0;
	v8::String::Utf8Value event_name(v8::Isolate::GetCurrent(),  args[iNameArg] );
	const char * pchEventName = *event_name;
	if( !pchEventName )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<512>( "Invalid event name to %s", pchFuncCall ).String() ).ToLocalChecked() );
		return;
	}

	float flAsyncDelay = 0.0f;
	if( bAsyncDispatch )
	{
		v8::Local<v8::Number> num = args[0]->ToNumber( GetV8Isolate()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Number>() );
		flAsyncDelay = num->Value();
	}

	// find parse function
	CPanoramaSymbol symEvent( pchEventName );
	int iMap = mapEvents.Find( symEvent );
	if( iMap == mapEvents.InvalidIndex() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<512>( "Invalid event name to DispatchEvent ('%s')", pchEventName ).String() ).ToLocalChecked() );
		return;
	}

	IUIPanel *pPanelTarget = UIEngine()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );

	int iFirstFuncArg = bAsyncDispatch ? 2 : 1;
	UIEventFactory &info = mapEvents.Element( iMap );
	if( args.Length() - iFirstFuncArg == (info.m_cParams + 1) )
	{
		// Parse next arg as target panel if we have an extra arg

		if( args[iFirstFuncArg]->IsObject() )
		{
			v8::Local<v8::Object> obj = args[iFirstFuncArg]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
			if( obj->InternalFieldCount() != 1 )
			{
				GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), CFmtStr1024( "Invalid panel target to %s of %s", pchFuncCall, pchEventName ).String() ).ToLocalChecked() );
				return;
			}

			v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() )->GetInternalField( 0 ) );
			IUIPanel *pPanel = (IUIPanel*)wrap->Value();
			if( UIEngine()->IsValidPanelPointer( pPanel ) )
			{
				pPanelTarget = pPanel;
			}
			else
			{
				args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "Invalid panel target to %s of %s", pchFuncCall, pchEventName ).String() ).ToLocalChecked() );
				return;
			}
		}
		else
		{
			v8::String::Utf8Value panel_name(v8::Isolate::GetCurrent(),  args[iFirstFuncArg] );
			const char * pchPanelTarget = *panel_name;
			if( pchPanelTarget )
			{
				if( V_stricmp( pPanelTarget->GetID(), pchPanelTarget ) != 0 )
				{
					IUIPanel *pChild = pPanelTarget->FindChildTraverse( pchPanelTarget );
					if( pChild )
						pPanelTarget = pChild;
					else
					{
						args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "Invalid panel target to %s of %s", pchFuncCall, pchEventName ).String() ).ToLocalChecked() );
						return;
					}
				}
			}
		}

		++iFirstFuncArg;
	}
	else if( args.Length() - iFirstFuncArg != info.m_cParams )
	{
		// wrong arg count to event, don't allow this
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "Invalid number of arguments for event in %s for %s", pchFuncCall, pchEventName ).String() ).ToLocalChecked() );
		return;
	}

	IUIEvent * pEvent = nullptr;
	if ( info.m_pfnParseUIEvent )
	{
		// Create event from string
		// Convert arguments to a string and create event from string
		
		CUtlStringBuilder strArgs;
		for ( int i = iFirstFuncArg; i < args.Length(); ++i )
		{
			v8::String::Utf8Value argval(v8::Isolate::GetCurrent(),  args[i] );
			const char *pchStringArg = *argval;

			if ( !pchStringArg )
			{
				args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "Argument %i to %s for event %s could not be converted to a string for parsing", i, pchFuncCall, pchEventName ).String() ).ToLocalChecked() );
				return;
			}

			// we dont know what type of arguments the event takes, so quote everything as strings (UIEvent is ok with that for non-string values)
			if ( i > iFirstFuncArg )
			{
				CUtlString strArg;
				EscapeUIEventParam( &strArg, pchStringArg );
				strArgs.AppendFormat( ", '%s' ", strArg.String() );
			}
			else
			{
				CUtlString strArg;
				EscapeUIEventParam( &strArg, pchStringArg );
				strArgs.AppendFormat( " '%s' ", strArg.String() );
			}
		}

		// parse
		PFN_ParseUIEvent func = info.m_pfnParseUIEvent;
		Assert( func );

		const char *pchEnd = NULL;
		pEvent = func( pPanelTarget, strArgs.String(), &pchEnd );
	}
	else
	{
		// Event defined in javascript
		// Create event from the javascript arguments

		CUtlVector< v8::Local< v8::Value > > eventArgs;
		for ( int i = iFirstFuncArg; i < args.Length(); ++i )
		{
			eventArgs.AddToTail( args[i] );
		}

		PFN_ParseUIEventJS func = info.m_pfnParseUIEventJS;
		Assert( func );

		pEvent = func( symEvent, pPanelTarget, eventArgs );
	}

	if( !pEvent )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "Event arguments could not be parsed for %s of %s", pchFuncCall, pchEventName ).String() ).ToLocalChecked() );
		return;
	}

	if( bAsyncDispatch )
	{
		UIEngine()->DispatchEventAsync( flAsyncDelay, pEvent );
	}
	else
	{
		bool bResult = UIEngine()->DispatchEvent( pEvent );

		v8::Handle< v8::Boolean > return_val = v8::Boolean::New( args.GetIsolate(), bResult );
		args.GetReturnValue().Set( return_val );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handler for javascript DispatchEvent calls
//-----------------------------------------------------------------------------
void JSDispatchEventAsync( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	JSDispatchEventHelper( args, true, UIEngineInternal()->MapRegisteredEvents() );
}


//-----------------------------------------------------------------------------
// Purpose: Handler for javascript DispatchEvent calls
//-----------------------------------------------------------------------------
void JSDispatchEvent( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	JSDispatchEventHelper( args, false, UIEngineInternal()->MapRegisteredEvents() );
}


//-----------------------------------------------------------------------------
// Purpose: Creates an event from string representation
//
//			For panel events, an extra parameter (first param) can be the ID of the panel to raise the event on. When present,
//			pCreatingPanel is used to find the panel
//-----------------------------------------------------------------------------
IUIEvent *CUIEngine::CreateEventFromString( IUIPanel *pCreatingPanel, const char *pchEvent, const char **pchEventEnd )
{
	if ( !pchEvent || !pchEventEnd )
		return NULL;

	// parse name
	char rgchName[256];
	int cch = V_strcspn( pchEvent, "(" );
	if( cch == 0 || pchEvent[cch] == '\0' )
		return NULL;

	V_strncpy( rgchName, pchEvent, cch + 1 ); // +1 for '\0'
	V_StrTrim( rgchName );
	CPanoramaSymbol symEvent( rgchName );

	// find parse function
	int iMap = m_mapEventRegistrations.Find( symEvent );
	if( iMap == m_mapEventRegistrations.InvalidIndex() )
		return NULL;

	const char *pchParamStart = &pchEvent[cch + 1];
	UIEventFactory &info = m_mapEventRegistrations.Element( iMap );

	// if this has 1 extra param, find the panel from the first param
	IUIPanel *pTarget = pCreatingPanel;
	if( CountUIEventParams( pchParamStart ) == (info.m_cParams + 1) )
	{
		// get first param
		const char *pchID = NULL;
		if( !ParseUIEventParam( &pchID, pCreatingPanel, pchParamStart, &pchParamStart ) )
			return NULL;

		pTarget = pCreatingPanel->FindPanelInLayoutFile( pchID );

		// Now check all parents, even if not in layout file as last resort, need some way
		// to fire events up at parents from children loaded in seperate layout file!
		IUIPanel *pParent = pCreatingPanel->GetParent();
		while( !pTarget && pParent )
		{
			if( !V_stricmp( pParent->GetID(), pchID ) )
				pTarget = pParent;
			else
				pParent = pParent->GetParent();
		}

		free( (void*)pchID );


		if( !pTarget )
			return NULL;
	}

	// parse
	if ( info.m_pfnParseUIEvent )
	{
		PFN_ParseUIEvent func = info.m_pfnParseUIEvent;
		return func( pTarget, pchParamStart, pchEventEnd );
	}
	else
	{
		if ( info.m_pfnParseUIEventJS )
		{
			Warning( "Unable to create event '%s' from string. If you a trying to dispatch the event from C++, make sure to declare/define the event in C++ (and not javascript).\n", rgchName );
		}
		return NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create multiple events from a string representation (whitespace
//			separated like you do in XML)
//-----------------------------------------------------------------------------
bool CUIEngine::CreateEventsFromString( VecUIEvents_t *pOutVecUIEvents, IUIPanel *pCreatingPanel, const char *pchEvent, const char **pchEventEnd )
{
	// We assert that the out-vector is empty because on failure we clear it
	// out and it would be weird to delete someone else's events. So don't
	// pass in a non-empty vector, please!
	Assert( pOutVecUIEvents && pOutVecUIEvents->IsEmpty() );

	const char *pchCur = pchEvent;
	while ( pchCur && pchCur[0] != '\0' )
	{
		const char *pchEnd = NULL;
		IUIEvent *pEvent = UIEngine()->CreateEventFromString( pCreatingPanel, pchCur, &pchEnd );
		if ( !pEvent || !pchEnd )
		{
			pOutVecUIEvents->PurgeAndDeleteElements();
			return false;
		}

		pchEnd = CSSHelpers::SkipSpaces( pchEnd );
		if ( pchEnd[0] == ';' )
			++pchEnd;
		pchEnd = CSSHelpers::SkipSpaces( pchEnd );

		pOutVecUIEvents->AddToTail( pEvent );
		pchCur = pchEnd;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Convert V8 string to const char *
//-----------------------------------------------------------------------------
const char* V8ToCString( const v8::String::Utf8Value& value ) 
{
	return *value ? *value : "<string conversion failed>";
}

/*
//-----------------------------------------------------------------------------
// Purpose: Callback when v8 frees panel objects
//-----------------------------------------------------------------------------
template <typename T> static void WeakPointerCallback( const v8::WeakCallbackData<T, IUIPanel * >&data )
{
	v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::New( GetV8Isolate(), data.GetValue() );
	data.GetValue().Clear();

	IUIPanel *pPanel = data.GetParameter();
	UIEngineInternal()->FreeV8PersistentPanelObjectInstance( pPanel );
}
*/


//-----------------------------------------------------------------------------
// Purpose: Helper for deleting v8 object instances
//-----------------------------------------------------------------------------
void CUIEngine::DeleteJSObjectInstance( IUIJSObject *pJSObjectInstance )
{
	v8::Isolate::Scope isolate_scope( UIEngineInternal()->GetV8Isolate() );
	v8::HandleScope handle_scope( UIEngineInternal()->GetV8Isolate() );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( UIEngineInternal()->GetV8Isolate(), m_V8UIEngineGlobalContext );

	v8::Context::Scope context_scope( context );

	int iMap = m_MapV8GlobalObjectInstances.Find( pJSObjectInstance );
	if( iMap != m_MapV8GlobalObjectInstances.InvalidIndex() )
	{
		v8::Persistent<v8::Object> * pPersistent = m_MapV8GlobalObjectInstances[iMap];
		v8::Local<v8::Object> obj = v8::Local<v8::Object>::New( m_pV8Isolate, *pPersistent );

		obj->SetInternalField( 0, v8::External::New( GetV8Isolate(), NULL ) );
		pPersistent->Reset();
		delete pPersistent;
		m_MapV8GlobalObjectInstances.RemoveAt( iMap );
	}
	else
	{
		// It's ok to find no instance.  We won't always have created one just because we are registered as a 
		// possible IUIJSObject, so don't assert here!!
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for creating and tracking new v8 object instances
//-----------------------------------------------------------------------------
v8::Persistent<v8::Object> *CUIEngine::CreateV8ObjectInstance( const char *pchObjectType, void *pActualObject, IUIJSObject *pJSObject )
{
	int iMap = m_MapV8GlobalObjectInstances.Find( pJSObject );
	if( iMap != m_MapV8GlobalObjectInstances.InvalidIndex() )
	{
		v8::Persistent<v8::Object> * pPersistent = m_MapV8GlobalObjectInstances[iMap];
		return pPersistent;
	}

	int iTemplateMap = m_mapV8ClassTemplatesByType.Find( pchObjectType );
	if( m_mapV8ClassTemplatesByType.InvalidIndex() == iTemplateMap )
	{
		AssertMsg1( false, "Object type %s is not registered in CreateV8ObjectInstance", pchObjectType );
		return NULL ;
	}

	v8::Persistent<v8::FunctionTemplate> *pTemplate = m_mapV8ClassTemplatesByType[iTemplateMap];
	v8::Local<v8::FunctionTemplate> classTemplate = v8::Local<v8::FunctionTemplate>::New( m_pV8Isolate, *pTemplate );

	v8::Local<v8::Object> obj = classTemplate->InstanceTemplate()->NewInstance( GetV8Isolate()->GetCurrentContext() ).ToLocalChecked();
	obj->SetInternalField( 0, v8::External::New( m_pV8Isolate, pActualObject ) );

	v8::Persistent<v8::Object> *persistentObject = new v8::Persistent<v8::Object>();
	persistentObject->Reset( m_pV8Isolate, obj );

	m_MapV8GlobalObjectInstances.Insert( pJSObject, persistentObject );

	return persistentObject;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a panel to be associated with a javascript context
//-----------------------------------------------------------------------------
void CUIEngine::AddPanelForV8Context( IUIPanel *pPanel, IUIPanel *pContext )
{
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlPtr< CUtlVector< IUIPanel* > > pvecPanels = m_mapOtherPanelsV8InContext.FindElement( pContext, NULL );
	if ( !pvecPanels.GetPtr() )
	{
		pvecPanels.SetPtr( new CUtlVector< IUIPanel* >() );
		m_mapOtherPanelsV8InContext.Insert( pContext, pvecPanels );
	}
#else
	CUtlPtr< CUtlVector< IUIPanel* > > &pvecPanels = *m_mapOtherPanelsV8InContext.FindOrInsertDefaultGetPtr( pContext );
	if ( !pvecPanels.GetPtr() )
	{
		pvecPanels.SetPtr( new CUtlVector< IUIPanel* >() );
	}
#endif	// PANORAMA_USE_S1WRAPPER

#if _DEBUG
	Assert( !pvecPanels->HasElement( pPanel ) );
#endif

	pvecPanels->AddToTail( pPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Removes a panel from being associated with a javascript context
//-----------------------------------------------------------------------------
void CUIEngine::RemovePanelForV8Context( IUIPanel *pPanel, IUIPanel *pContext )
{
	int iMap = m_mapOtherPanelsV8InContext.Find( pContext );
	if ( iMap == m_mapOtherPanelsV8InContext.InvalidIndex() )
		return;	

	CUtlPtr< CUtlVector< IUIPanel* > > pvecPanels = m_mapOtherPanelsV8InContext.Element( iMap );
	pvecPanels->FindAndFastRemove( pPanel );

	if ( pvecPanels->IsEmpty() )
		m_mapOtherPanelsV8InContext.RemoveAt( iMap );
}


//-----------------------------------------------------------------------------
// Purpose: Returns a vector of panels associated with this V8 context
//-----------------------------------------------------------------------------
CUtlVector< IUIPanel * > *CUIEngine::GetAssociatedPanelsForV8Context( IUIPanel *pContext )
{
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlPtr< CUtlVector< IUIPanel* > > pvecPanels = m_mapOtherPanelsV8InContext.FindElement( pContext, NULL );
	return pvecPanels.GetPtr();
#else
	CUtlPtr< CUtlVector< IUIPanel* > > *pvecPanels = m_mapOtherPanelsV8InContext.FindGetPtr( pContext );
	return pvecPanels ? pvecPanels->GetPtr() : nullptr;
#endif	// PANORAMA_USE_S1WRAPPER
}


//-----------------------------------------------------------------------------
// Purpose: Returns a vector of panels associated with this V8 context
//-----------------------------------------------------------------------------
void CUIEngine::DeleteAssociatedPanelsForV8Context( IUIPanel *pContext )
{
	int iMap = m_mapOtherPanelsV8InContext.Find( pContext );
	if ( iMap == m_mapOtherPanelsV8InContext.InvalidIndex() )
		return;

	CUtlPtr< CUtlVector< IUIPanel* > > pvecPanels = m_mapOtherPanelsV8InContext.Element( iMap );

	// remove panels from map right away so destructors calling RemovePanelForV8Context() wont modify list. Remove call will just silently return
	m_mapOtherPanelsV8InContext.RemoveAt( iMap );

	CUtlVector< IUIPanel * > *pvecPanelsRaw = pvecPanels.GetPtr();
	FOR_EACH_VEC( *pvecPanelsRaw, i )
	{
		pvecPanels->Element( i )->ClientPtr()->OnDeletePanel();
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Helper to convert JS style prop name to actual style prop name
//-----------------------------------------------------------------------------
void ConvertJSStylePropNameToRealName( const char *pchPropIn, CUtlStringBuilder &strOut )
{
	int strLen = (int)V_strlen( pchPropIn );

	// + 4 so we don't reallocate even if the prop had four -'s in it's name.  
	strOut.Clear();
	strOut.EnsureCapacity( strLen + 4 );

	int toLower = 'a' - 'A';
	for( int i = 0; i < strLen; ++i )
	{
		char c = pchPropIn[i];
		if( c >= 'A' && c <= 'Z' )
		{
			strOut.AppendChar( '-' );
			strOut.AppendChar( c + toLower );
		}
		else
			strOut.AppendChar( c );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper to get element style attributes
//-----------------------------------------------------------------------------
void JSPanelStyleGet( v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info )
{
	if( info.Holder()->InternalFieldCount() != 1 )
	{
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "Property getter for CPanelStyle called on invalid object" ).ToLocalChecked() );
		return;
	}

	CPanelStyle *pStyle = NULL;
	V8ParamToPanoramaType( info.Holder(), &pStyle );
	if( !pStyle )
		return;

	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  property );
	const char *pchPropJS = *str;
	if( !pchPropJS )
	{
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Property setter for CPanelStyle called with invalid css property name (%s)", pchPropJS ).String() ).ToLocalChecked() );
		return;
	}

	// Special prop don't fail but return nothing
	if( V_stricmp( pchPropJS, "IsValid" ) == 0 )
	{
		return;
	}

	CUtlStringBuilder strProp;
	ConvertJSStylePropNameToRealName( pchPropJS, strProp );

	CStyleSymbol symParsedName( strProp.String() );
	CStyleSymbol symRealPropertyName = CStylePropertyFactory::GetPropertyNameForAlias( symParsedName );
	if( !symRealPropertyName.IsValid() )
	{
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Property setter for CPanelStyle called with invalid css property name (%s)", pchPropJS ).String() ).ToLocalChecked() );
		return;
	}

	if( symParsedName != symRealPropertyName )
	{
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), "You can set but not get style alias names, you must get on the base style property name" ).ToLocalChecked() );
		return;
	}

	CStyleProperty * pProp = pStyle->GetPropertyFromElementStyle( symRealPropertyName );
	if( !pProp )
	{
		info.GetReturnValue().Set( v8::Null( info.GetIsolate() ) );
		return;
	}

	CFmtStr1024 strText;
	pProp->ToString( &strText );

	v8::Handle<v8::Value> p;
	PanoramaTypeToV8Param( strText.String(), &p );
	info.GetReturnValue().Set( p );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to set element style attributes
//-----------------------------------------------------------------------------
void JSPanelStyleSet( v8::Local<v8::Name> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value> &info )
{
	if( info.Holder()->InternalFieldCount() != 1 )
	{
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "Property setter for CPanelStyle called on invalid object" ).ToLocalChecked() );
		return;
	}

	CPanelStyle *pStyle = NULL;
	V8ParamToPanoramaType( info.Holder(), &pStyle );
	if( !pStyle )
		return;

	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  property );
	const char *pchPropJS = *str;
	if( !pchPropJS )
	{
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Property setter for CPanelStyle called with invalid css property name (%s)", pchPropJS ).String() ).ToLocalChecked() );
		return;
	}

	CUtlStringBuilder strProp;
	ConvertJSStylePropNameToRealName( pchPropJS, strProp );
	CStyleSymbol symParsedName( strProp.String() );
	CStyleSymbol symRealPropertyName = CStylePropertyFactory::GetPropertyNameForAlias( symParsedName );

	// If someone tried to set NULL, then that means unset the element property, but this doesn't work on alias's since we only support partial set of them
	if( value->IsNull() )
	{
		if( symParsedName != symRealPropertyName )
		{
			info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), "Cannot set a property alias to undefined, only a base property, we don't support unset of alias alone" ).ToLocalChecked() );
			return;
		}
		pStyle->ClearPropertySetFromElement( symParsedName );
		return;
	}

	v8::String::Utf8Value strValue(v8::Isolate::GetCurrent(),  value->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
	const char *pchStrValue = *strValue;
	if( !pchStrValue )
	{
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Property setter for CPanelStyle called with empty string as value for %s", pchPropJS ).String() ).ToLocalChecked() );
		return;
	}

	if( !symRealPropertyName.IsValid() )
	{
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Property setter for CPanelStyle called with invalid css property name (%s)", pchPropJS ).String() ).ToLocalChecked() );
		return;
	}

	CStyleProperty *pNewProp = CStylePropertyFactory::CreateStyleProperty( symRealPropertyName );

	// get the value for this property. Include spaces
	CUtlBuffer buffer( pchStrValue, V_strlen( pchStrValue )+1, CUtlBuffer::READ_ONLY );
	char rgchStyleBuf[1024];
	if ( !CSSHelpers::BReadCSSToken( buffer, rgchStyleBuf, V_ARRAYSIZE( rgchStyleBuf ), k_rgchCSSValueTermOrEndOfString, V_ARRAYSIZE( k_rgchCSSValueTermOrEndOfString ) ) || rgchStyleBuf[0] == '\0' )
	{
		CStylePropertyFactory::FreeStyleProperty( pNewProp );
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Failed to parse style value for %s", pchPropJS ).String() ).ToLocalChecked() );
		return;
	}

	// NOTE no define replacing on the value, we don't know what the style file is so doesn't make sense!

	// try and set the property values. Make sure we pass in the parsed name, so the property knows what data to expect
	if( !pNewProp->BSetFromString( symParsedName, rgchStyleBuf ) )
	{
		CStylePropertyFactory::FreeStyleProperty( pNewProp );
		info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Failed to set property value (property=%s)(value=%s)", symParsedName.String(), rgchStyleBuf ).String() ).ToLocalChecked() );
		return;
	}

	// Merge the element style with any existing property
	pStyle->MergeProperty( pNewProp );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to create a JS object to wrap a given panel style
//-----------------------------------------------------------------------------
v8::Persistent<v8::Object> *CUIEngine::CreateV8PanelStyleInstance( IUIPanelStyle *pPanelStyle )
{
	IUIPanel *pPanel = ((CPanelStyle*)pPanelStyle)->AccessPanel();
	int iMap = m_MapV8PanelStyleObjectInstances.Find( pPanel );
	if( iMap != m_MapV8PanelStyleObjectInstances.InvalidIndex() )
	{
		return m_MapV8PanelStyleObjectInstances[iMap];
	}

	if( m_V8PanelStyleTemplate.IsEmpty() )
	{
		v8::Handle<v8::ObjectTemplate> panelstyle = v8::ObjectTemplate::New( m_pV8Isolate );
		panelstyle->SetInternalFieldCount( 1 );

		// Bind some global functions for use from JS
		panelstyle->SetHandler( v8::NamedPropertyHandlerConfiguration( JSPanelStyleGet, JSPanelStyleSet, nullptr, nullptr, nullptr, v8::Local<v8::Value>(), v8::PropertyHandlerFlags::kOnlyInterceptStrings ) );
		panelstyle->Set( v8::String::NewFromUtf8( GetV8Isolate(), "IsValid" ).ToLocalChecked(), v8::FunctionTemplate::New( GetV8Isolate(), &JSCheckObjectValidity ) );

		m_V8PanelStyleTemplate.Reset( m_pV8Isolate, panelstyle );
	}

	v8::Handle<v8::ObjectTemplate> objTemplate = v8::Local<v8::ObjectTemplate>::New( m_pV8Isolate, m_V8PanelStyleTemplate );
	
	v8::Local<v8::Object> obj = objTemplate->NewInstance( GetV8Isolate()->GetCurrentContext() ).ToLocalChecked();
	obj->SetInternalField( 0, v8::External::New( m_pV8Isolate, pPanel ) );

	v8::Persistent<v8::Object> *persistentObject = new v8::Persistent<v8::Object>();
	persistentObject->Reset( m_pV8Isolate, obj );

	m_MapV8PanelStyleObjectInstances.Insert( pPanel, persistentObject );

	return persistentObject;
}



//-----------------------------------------------------------------------------
// Purpose: Helper to create a JS object to wrap a given ui window
//-----------------------------------------------------------------------------
v8::Persistent<v8::Object> *CUIEngine::CreateV8IUIWindowInstance( IUIWindow *pUIWindow )
{
	int iMap = m_MapV8IUIWindowObjectInstances.Find( pUIWindow );
	if ( iMap != m_MapV8IUIWindowObjectInstances.InvalidIndex() )
	{
		return m_MapV8IUIWindowObjectInstances[iMap];
	}

	if ( m_V8PanelStyleTemplate.IsEmpty() )
	{
		v8::Handle<v8::ObjectTemplate> panelstyle = v8::ObjectTemplate::New( m_pV8Isolate );
		panelstyle->SetInternalFieldCount( 1 );

		// Bind some global functions for use from JS
		panelstyle->SetHandler( v8::NamedPropertyHandlerConfiguration( JSPanelStyleGet, JSPanelStyleSet, nullptr, nullptr, nullptr, v8::Local<v8::Value>(), v8::PropertyHandlerFlags::kOnlyInterceptStrings ) );
		panelstyle->Set( v8::String::NewFromUtf8( GetV8Isolate(), "IsValid" ).ToLocalChecked(), v8::FunctionTemplate::New( GetV8Isolate(), &JSCheckObjectValidity ) );

		m_V8PanelStyleTemplate.Reset( m_pV8Isolate, panelstyle );
	}

	v8::Handle<v8::ObjectTemplate> objTemplate = v8::Local<v8::ObjectTemplate>::New( m_pV8Isolate, m_V8PanelStyleTemplate );

	v8::Local<v8::Object> obj = objTemplate->NewInstance( GetV8Isolate()->GetCurrentContext() ).ToLocalChecked();
	obj->SetInternalField( 0, v8::External::New( m_pV8Isolate, pUIWindow ) );

	v8::Persistent<v8::Object> *persistentObject = new v8::Persistent<v8::Object>();
	persistentObject->Reset( m_pV8Isolate, obj );

	m_MapV8IUIWindowObjectInstances.Insert( pUIWindow, persistentObject );

	return persistentObject;
}


//-----------------------------------------------------------------------------
// Purpose: Helper for creating and tracking new v8 panel object instance
//-----------------------------------------------------------------------------
v8::Persistent<v8::Object> *CUIEngine::CreateV8PanelInstance( IUIPanel *pPanel )
{
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	int iMap = m_MapV8PanelObjectInstances.Find( pPanel );
	if( iMap != m_MapV8PanelObjectInstances.InvalidIndex() )
	{
		return m_MapV8PanelObjectInstances[iMap];
	}

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pPanel );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *pContextRef );
	v8::Context::Scope context_scope( context );

	v8::Persistent<v8::FunctionTemplate> *pTemplate = UIEngineInternal()->GetJSClassTemplateForPanel( pPanel );
	v8::Handle<v8::FunctionTemplate> classTemplate = v8::Local<v8::FunctionTemplate>::New( m_pV8Isolate, *pTemplate );

	v8::Local<v8::Object> obj = classTemplate->InstanceTemplate()->NewInstance( GetV8Isolate()->GetCurrentContext() ).ToLocalChecked();


	obj->SetInternalField( 0, v8::External::New( m_pV8Isolate, pPanel ) );

	v8::Persistent<v8::Object> *persistentObject = new v8::Persistent<v8::Object>();
	persistentObject->Reset( m_pV8Isolate, obj );
	
	/*
	// bugbug jmccaskey - need to get this working, so we can delete some memory earlier than on panel delete, not ultra critical.

	WeakObjectPtr_t *pwcb = new WeakObjectPtr_t;
	pwcb->pPanel = pPanel;
	pwcb->pPersistent = persistentObject;

	persistentObject->SetWeak( pwcb, &WeakPointerCallback );
	*/
	
	m_MapV8PanelObjectInstances.Insert( pPanel, persistentObject );

	return persistentObject;
}

//-----------------------------------------------------------------------------
// Purpose: Handler for javascript Msg() calls
//-----------------------------------------------------------------------------
void JSMsg( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	CUtlStringBuilder strMsg;
	for ( int iArg = 0; iArg < args.Length(); ++iArg )
	{
		v8::HandleScope handle_scope( args.GetIsolate() );
		v8::Local< v8::Value > arg = args[ iArg ];
		if ( arg->IsFunction() )
		{
			v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast( arg );
			v8::String::Utf8Value funcName(v8::Isolate::GetCurrent(),  func->GetName() );
			strMsg.AppendFormat( "function %s()", *funcName );
		}
		else if ( arg->IsObject() )
		{
			v8::Handle< v8::String > str = panorama::JSObjectToJSON( args.GetIsolate(), arg.As< v8::Object >() );
			v8::String::Utf8Value strVal(v8::Isolate::GetCurrent(),  str );
			strMsg.Append( *strVal );
		}
		else
		{
			v8::String::Utf8Value strVal(v8::Isolate::GetCurrent(),  arg );
			strMsg.Append( *strVal );
		}
	}

	UIEngineInternal()->OutputJSString( UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) ), strMsg.String() );
}


//-----------------------------------------------------------------------------
// Purpose: Handler for javascript RegisterEventHandler() calls
//-----------------------------------------------------------------------------
void JSRegisterEventHandler( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	// function myfunc( event ) { } 
	// RegisterEventHandler( 'ShowMessageBox', 'PanelID', myfunc );
	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );

	if( args.Length() != 3 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Wrong number of arguments to RegisterEventHandler" ).ToLocalChecked() );
		return;
	}

	if( !args[0]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to RegisterEventHandler must be a string (event type)" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value event_name(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchEventName = *event_name;
	if( !pchEventName || !UIEngine()->IsValidEventName( pchEventName ) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to RegisterEventHandler is not a valid event type" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pPanelToRegisterOn = NULL;
	if( args[1]->IsObject() )
	{
		v8::Local<v8::Object> obj = args[1]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		if( obj->InternalFieldCount() != 1 )
		{
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "Second argument to RegisterEventHandler must be a string (panel id) or panel object (was an object, but not a valid panel object)" ).ToLocalChecked() );
			return;
		}

		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
		IUIPanel *pPanel = (IUIPanel*)wrap->Value();
		if( UIEngineInternal()->IsValidPanelPointer( pPanel ) )
		{
			pPanelToRegisterOn = pPanel;
		}
		else
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to RegisterEventHandler must be a string (panel id) or panel object (was an object, but not a valid panel object)" ).ToLocalChecked() );
			return;
		}
	}

	if( !pPanelToRegisterOn && !args[1]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to RegisterEventHandler must be a string (panel id) or valid panel object" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pContextPanel = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	if( !pContextPanel )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "RegisterEventHandler called outside panel context, can't register on a panel except in a containing panel/xml layout context" ).ToLocalChecked() );
		return;
	}

	if( !pPanelToRegisterOn )
	{
		v8::String::Utf8Value panel_id(v8::Isolate::GetCurrent(),  args[1] );
		const char * pchPanelID = *panel_id;
		if( !pchPanelID )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to RegisterEventHandler is not a valid panel id" ).ToLocalChecked() );
			return;
		}

		if( !args[2]->IsFunction() )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Third argument to RegisterEventHandler is not a valid callback function" ).ToLocalChecked() );
			return;
		}
	
		if( V_strcmp( pContextPanel->GetID(), pchPanelID ) == 0 )
			pPanelToRegisterOn = pContextPanel;
		else
			pPanelToRegisterOn = pContextPanel->FindChildTraverse( pchPanelID );

		if( !pPanelToRegisterOn )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "RegisterEventHandler could not find panel (%s) to register on within context (%s -> %s)", pchPanelID, pContextPanel->GetID(), pContextPanel->GetLayoutFile().String() ).String() ).ToLocalChecked() );
			return;
		}
	}
	
	v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast( args[2] );
	v8::Persistent<v8::Function> *pfn = new v8::Persistent<v8::Function>( args.GetIsolate(), func );
	
	uint32 unHandlerId = UIEngineInternal()->RegisterJSEventHandler( pchEventName, pPanelToRegisterOn, pContextPanel, pfn );
	args.GetReturnValue().Set( unHandlerId );
}

//-----------------------------------------------------------------------------
// Purpose: Handler for javascript UnregisterEventHandler() calls
//-----------------------------------------------------------------------------
void JSUnregisterEventHandler( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	// function myfunc( event ) { } 
	// UnregisterEventHandler( 'ShowMessageBox', 'PanelID', handleId );
	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );

	if( args.Length() != 3 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Wrong number of arguments to UnregisterEventHandler" ).ToLocalChecked() );
		return;
	}

	if( !args[0]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to UnregisterEventHandler must be a string (event type)" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value event_name(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchEventName = *event_name;
	if( !pchEventName || !UIEngine()->IsValidEventName( pchEventName ) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to UnregisterEventHandler is not a valid event type" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pPanelToRegisterOn = NULL;
	if( args[1]->IsObject() )
	{
		v8::Local<v8::Object> obj = args[1]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		if( obj->InternalFieldCount() != 1 )
		{
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "Second argument to UnregisterEventHandler must be a string (panel id) or panel object (was an object, but not a valid panel object)" ).ToLocalChecked() );
			return;
		}

		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
		IUIPanel *pPanel = (IUIPanel*)wrap->Value();
		if( UIEngineInternal()->IsValidPanelPointer( pPanel ) )
		{
			pPanelToRegisterOn = pPanel;
		}
		else
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to UnregisterEventHandler must be a string (panel id) or panel object (was an object, but not a valid panel object)" ).ToLocalChecked() );
			return;
		}
	}

	if( !pPanelToRegisterOn && !args[1]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to UnregisterEventHandler must be a string (panel id) or valid panel object" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pContextPanel = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	if( !pContextPanel )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "UnregisterEventHandler called outside panel context, can't register on a panel except in a containing panel/xml layout context" ).ToLocalChecked() );
		return;
	}

	if( !pPanelToRegisterOn )
	{
		v8::String::Utf8Value panel_id(v8::Isolate::GetCurrent(),  args[1] );
		const char * pchPanelID = *panel_id;
		if( !pchPanelID )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to UnRegisterEventHandler is not a valid panel id" ).ToLocalChecked() );
			return;
		}

		if( V_strcmp( pContextPanel->GetID(), pchPanelID ) == 0 )
			pPanelToRegisterOn = pContextPanel;
		else
			pPanelToRegisterOn = pContextPanel->FindChildTraverse( pchPanelID );

		if( !pPanelToRegisterOn )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "UnregisterEventHandler could not find panel (%s) to register on within context (%s -> %s)", pchPanelID, pContextPanel->GetID(), pContextPanel->GetLayoutFile().String() ).String() ).ToLocalChecked() );
			return;
		}
	}

	uint32 unHandlerId = args[2]->Uint32Value( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( 0 );

	UIEngineInternal()->UnregisterJSEventHandler( pchEventName, pPanelToRegisterOn, unHandlerId );
}


//-----------------------------------------------------------------------------
// Purpose: Handler for javascript RegisterEventHandler() calls
//-----------------------------------------------------------------------------
void JSRegisterForUnhandledEvent( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	v8::Isolate::Scope isolate_scope( args.GetIsolate() );
	v8::HandleScope handle_scope( args.GetIsolate() );

	if ( args.Length() != 2 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Wrong number of arguments to RegisterForUnhandledEvent" ).ToLocalChecked() );
		return;
	}

	if ( !args[0]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to RegisterForUnhandledEvent must be a string (event type)" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value event_name(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchEventName = *event_name;
	if ( !pchEventName || !UIEngine()->IsValidEventName( pchEventName ) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to RegisterForUnhandledEvent is not a valid event type" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pContextPanel = UIEngine()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	if ( !pContextPanel )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "RegisterForUnhandledEvent called outside panel context, can't register on a panel except in a containing panel/xml layout context" ).ToLocalChecked() );
		return;
	}

	v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast( args[1] );
	v8::Persistent<v8::Function> *pfn = new v8::Persistent<v8::Function>( args.GetIsolate(), func );

	uint32 unHandlerId = UIEngineInternal()->RegisterJSForUnhandledEvent( pchEventName, pContextPanel, pfn );
	args.GetReturnValue().Set( unHandlerId );
}


//-----------------------------------------------------------------------------
// Purpose: Handler for javascript RegisterEventHandler() calls
//-----------------------------------------------------------------------------
void JSUnregisterForUnhandledEvent( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	v8::HandleScope handle_scope( args.GetIsolate() );

	if ( args.Length() != 2 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Wrong number of arguments to UnregisterForUnhandledEvent" ).ToLocalChecked() );
		return;
	}

	if ( !args[0]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to UnregisterForUnhandledEvent must be a string (event type)" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value event_name(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchEventName = *event_name;
	if ( !pchEventName || !UIEngine()->IsValidEventName( pchEventName ) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to UnregisterForUnhandledEvent is not a valid event type" ).ToLocalChecked() );
		return;
	}

	if( !args[1]->IsUint32() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to UnregisterForUnhandledEvent must be an unsigned integer type (as returned from RegisterForUnhandledEvent)" ).ToLocalChecked() );
		return;
	}
	uint32 unHandlerId = args[1]->Uint32Value( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( 0 );

	UIEngineInternal()->UnregisterJSForUnhandledEvent( pchEventName, unHandlerId );
}


//-----------------------------------------------------------------------------
// Purpose:  Helper to expose delayed function calls to JS
//-----------------------------------------------------------------------------
bool CUIEngine::OnJSScheduledFunction( CPanelPtr< IUIPanel > panelContext, v8::Persistent<v8::Function> *pJSFunc, int nLayoutReloadCount, uint hScheduled )
{
	VPROF_BUDGET( "CUIEngine::OnJSScheduledFunction", VPROF_BUDGETGROUP_TENFOOT );

	// If panel context is gone, don't try to call
	IUIPanel *pPanel = panelContext.Get();
	if( pPanel == NULL )
	{
		return true;
	}

	// if the panel has been reloaded, don't call
	if ( pPanel->GetLayoutFileReloadCount() != nLayoutReloadCount )
	{
		return true;
	}

	// canceled by user?
	if ( !m_treeScheduledJSHandles.Remove( hScheduled ) )
		return true;

	v8::Isolate::Scope isolate_scope( UIEngineInternal()->GetV8Isolate() );
	v8::HandleScope handle_scope( UIEngineInternal()->GetV8Isolate() );

	v8::Persistent<v8::Context> *pContextRef = UIEngineInternal()->GetContextForPanel( pPanel );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( UIEngineInternal()->GetV8Isolate(), *pContextRef );

	v8::Context::Scope context_scope( context );
	v8::Handle<v8::Object> obj = context->Global();
	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), *(pJSFunc) );

	RunJSFunctionInternal( pPanel, context, obj, fnLocal, 0, nullptr, false );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the next available scheduled JS handle
//-----------------------------------------------------------------------------
uint CUIEngine::GetNextScheduledJSHandle()
{
	uint unNext = m_unNextScheduledJSHandle++;

#if defined(PANORAMA_USE_S1WRAPPER)

	if ( ( m_treeScheduledJSHandles.Find( unNext ) == m_treeScheduledJSHandles.InvalidIndex() ) )
	{
		m_treeScheduledJSHandles.Insert( unNext );
	}

#elif defined( SOURCE2_PANORAMA )
	m_treeScheduledJSHandles.Insert( unNext, k_eInsertUpdateDupes );
#else
	m_treeScheduledJSHandles.Insert( unNext, false );
#endif
	return unNext;
}


//-----------------------------------------------------------------------------
// Purpose: Cancels a scheduled function from JS
//-----------------------------------------------------------------------------
bool CUIEngine::BCancelScheduledJSHandle( uint hScheduled )
{
	return m_treeScheduledJSHandles.Remove( hScheduled );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to expose delayed function calls to JS
//-----------------------------------------------------------------------------
void JSSchedule( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	v8::Handle<v8::Value> retVal;

	if( args.Length() < 2 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Schedule() must be called with at least 2 arguments" ).ToLocalChecked() );
		return;
	}

	v8::Handle<v8::Number> delay = args[0]->ToNumber( GetV8Isolate()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Number>() );
	if( delay.IsEmpty() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to Schedule() must be a number as a delay in seconds" ).ToLocalChecked() );
		return;
	}

	if( !args[1]->IsFunction() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to Schedule() must be a function" ).ToLocalChecked() );
		return;
	}
	v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast( args[1] );

	v8::Persistent<v8::Function> *pFunc = new v8::Persistent<v8::Function>();
	pFunc->Reset( UIEngineInternal()->GetV8Isolate(), func );
	

	CPanelPtr<IUIPanel> pContext = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );

	uint hScheduled = UIEngineInternal()->GetNextScheduledJSHandle();
	::DispatchEventAsync( (float)delay->Value(), JSScheduledFunction(), (IUIPanel*)NULL, pContext, pFunc, pContext->GetLayoutFileReloadCount(), hScheduled );

	pFunc->Reset();
	delete pFunc;

	args.GetReturnValue().Set( hScheduled );
}


//-----------------------------------------------------------------------------
// Purpose: Cancels a scheduled function
//-----------------------------------------------------------------------------
void JSCancelScheduled( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( args.Length() < 1 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "CancelScheduled() must be called with 1 argument" ).ToLocalChecked() );
		return;
	}

	v8::Handle<v8::Uint32> hScheduled = args[0]->ToUint32( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Uint32>() );
	if ( hScheduled.IsEmpty() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to CancelScheduled() must be the handle returned frome Schedule()" ).ToLocalChecked() );
		return;
	}

	if ( !UIEngineInternal()->BCancelScheduledJSHandle( hScheduled->Uint32Value( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( 0 ) ) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to CancelScheduled() must be the handle returned frome Schedule()" ).ToLocalChecked() );
		return;
	}
}

// NOTE! Returns the panel at the top of the context panel stack. C++ code must call PushContextPanel and PopContextPanel 
// before calling JS
void JSGetContextPanel( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	IUIPanel *pPanel = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	
	if( pPanel )
	{
		v8::Handle<v8::Value> p;
		PanoramaTypeToV8Param( pPanel, &p );
		args.GetReturnValue().Set( p );
	}
	else
	{
		args.GetReturnValue().Set( v8::Null( GetV8Isolate() ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper to expose client language to JS
//-----------------------------------------------------------------------------
void JSLanguage( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	v8::Handle<v8::Value> retVal;

#if defined( SOURCE2_PANORAMA )
	PanoramaTypeToV8Param( g_pApplication->GetLanguage( LanguageType_UI ), &retVal );
#else
	ELanguage eLang = UIEngine()->GetCurrentInputLocale();
	PanoramaTypeToV8Param( GetLanguageShortName( eLang ), &retVal );
#endif
	args.GetReturnValue().Set( retVal );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for panorama Localize call from JS
//-----------------------------------------------------------------------------
void JSLocalize( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if( args.Length() < 1 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Localize() requires at least one argument" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value token(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchLocToken = *token;
	if( !pchLocToken )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Localize() requires a string as it's first argument" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pPanel = NULL;

	if( args.Length() >= 2 )
	{
		if( args[1]->IsObject() )
		{
			v8::Local<v8::Object> obj = args[1]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
			if( obj->InternalFieldCount() != 1 )
			{
				GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "Localize() got invalid optional panel value as second argument" ).ToLocalChecked() );
				return;
			}

			v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
			pPanel = (IUIPanel*)wrap->Value();
			if( !UIEngineInternal()->IsValidPanelPointer( pPanel ) )
			{
				args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Localize() got invalid optional panel value as second argument" ).ToLocalChecked() );
				return;
			}
		}
		else
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Localize() has an optional second panel argument as context for localize, but if set must be a panel object" ).ToLocalChecked() );
			return;
		}
	}

	const ILocalizationString *pchTitle = UILocalize()->PchFindToken( pPanel, pchLocToken, k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None, k_eStringEscapeStyle_None, true );

	v8::Handle<v8::Value> retVal;
	PanoramaTypeToV8Param( pchTitle->String(), &retVal );
	args.GetReturnValue().Set( retVal );
	pchTitle->Release();
}


//-----------------------------------------------------------------------------
// Purpose: Helper for panorama panel constructors
//-----------------------------------------------------------------------------
void JSPanelConstructor( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if( args.Length() < 3 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "CreatePanel requires at least 3 params (type,parent,id,[optional]properties)" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value type(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchPanelType = *type;
	if( !pchPanelType || !UIEngine()->BRegisteredPanelType( pchPanelType ) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "CreatePanel first param must be valid panel type with factory, %s is not valid or not registered to allow factory construction", pchPanelType ).String() ).ToLocalChecked() );
		return;
	}

	if( !args[1]->IsObject() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "CreatePanel second param (parent) must be a panel object" ).ToLocalChecked() );
		return;
	}

	if( !args[2]->IsString() && args[2]->IsNull() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "CreatePanel third param (id) must be string or undefined" ).ToLocalChecked() );
		return;
	}
	
	v8::String::Utf8Value id(v8::Isolate::GetCurrent(),  args[2] );
	const char * pchID = *id;

	v8::Local<v8::Object> obj = args[1]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
	if( obj->InternalFieldCount() != 1 )
	{
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "CreatePanel second param (parent) was not a valid panel object" ).ToLocalChecked() );
		return;
	}

	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
	IUIPanel *pPanel = (IUIPanel*)wrap->Value();
	if( !UIEngineInternal()->IsValidPanelPointer( pPanel ) )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "CreatePanel second param (parent) was not a valid panel object" ).ToLocalChecked() );
		return;
	}

	// Treat '' id as NULL so we don't allocate a string for the panel
	if( pchID && V_strlen( pchID ) == 0 )
		pchID = NULL;

	panorama::IUIPanelClient *pClientPanel = UIEngine()->CreatePanelClient( pchPanelType, pchID, pPanel );
	if ( !pClientPanel )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr( "CreatePanelClient - failed to create panel type %s", pchPanelType ) ).ToLocalChecked() );
		return;
	}

	IUIPanel *pNewChild = pClientPanel->UIPanel();
	if( !pNewChild )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Internal failure creating panel in panorama" ).ToLocalChecked() );
		return;
	}

	CPanelPtr<IUIPanel> pCallerContext = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	IUIPanel *pJSCallerContext = pCallerContext.Get();
	IUIPanel *pJSContext = pNewChild->GetJavaScriptContextParent();
	if ( pJSContext )
	{
		pNewChild->SetLayoutLoadedFromParent( pJSContext );

		// if the created panel has a context that differs from the caller, set caller as the js check context (so panels with local layout files can be checked against panels with web contexts)
		if ( pJSCallerContext && pJSContext != pJSCallerContext )
			pNewChild->SetLayoutFilePathForJSCheck( pJSCallerContext->GetLayoutFilePathForJSCheck() );


		// Optional - Set properties / panel events stored in the 4th argument
		// 4th argument should be a v8 object where each key represents a property
		// e.g. (a TextButton) { class:"MyClass", text:"Button", onactivate:"$.Msg('Button Pressed')" }
		if ( args.Length() > 3 )
		{
			if ( args[3]->IsObject() )
			{
				v8::Local<v8::Object> propObj = args[3]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
				v8::Local< v8::Array > keys = propObj->GetPropertyNames( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Array>() );

				CUtlVector< CUtlString > vecPropertyValues;
				CUtlVector< ParsedPanelProperty_t > vecPanelProperties;
				CUtlVector< PanelEventsToParse_t > vecPanelEvents;
				vecPropertyValues.EnsureCapacity( keys->Length() );
				vecPanelProperties.EnsureCapacity( keys->Length() );
				vecPanelEvents.EnsureCapacity( keys->Length() );

				const CPanoramaSymbol k_symPropertyClass( "class" );

				for ( uint32_t nProperty = 0; nProperty < keys->Length(); ++nProperty )
				{
					v8::Local< v8::Value > key = keys->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  nProperty ).FromMaybe( v8::Local<v8::Value>() );
					v8::Local< v8::Value > val = propObj->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  key ).FromMaybe( v8::Local<v8::Value>() );
					v8::String::Utf8Value propertyName(v8::Isolate::GetCurrent(),  key );
					v8::String::Utf8Value propertyValue(v8::Isolate::GetCurrent(),  val );

					if ( *propertyName && *propertyValue )
					{
						int nPropertyValue = vecPropertyValues.AddToTail( *propertyValue );
						const char * pchpropertyValue = vecPropertyValues[nPropertyValue].Get();
						CPanoramaSymbol symProperty( *propertyName );

						if ( symProperty == k_symPropertyClass )
						{
							pNewChild->AddClasses( pchpropertyValue );
						}
						else if ( pNewChild->BIsPanelEvent( symProperty ) )
						{
							PanelEventsToParse_t &panelEvent = vecPanelEvents[vecPanelEvents.AddToTail()];
							panelEvent.m_pPanel = pNewChild;
							panelEvent.m_symProperty = symProperty;
							panelEvent.m_pchEvent = pchpropertyValue;
						}
						else
						{
							ParsedPanelProperty_t &prop = vecPanelProperties[vecPanelProperties.AddToTail()];
							prop.m_symName = symProperty;
							prop.m_pchValue = pchpropertyValue;
						}
					}
					else
					{
						args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "CreatePanel - Properties: property values must be convertible to strings." ).ToLocalChecked() );
						return;
					}
				}

				pClientPanel->BSetProperties( vecPanelProperties );

				FOR_EACH_VEC( vecPanelEvents, i )
				{
					PanelEventsToParse_t &panelEvent = vecPanelEvents[i];
					if ( !panelEvent.m_pPanel->BParsePanelEvent( panelEvent.m_symProperty, panelEvent.m_pchEvent, pJSContext ) )
					{
						args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr( "CreatePanel - Failed to parse panel event, event=%s: %s\n", panelEvent.m_symProperty.String(), panelEvent.m_pchEvent ) ).ToLocalChecked() );
						return;
					}
				}

			}
			else
			{
				args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "CreatePanel - Unable to set properties. fourth param must be an object e.g. { class:\"MyClass\", text:\"Button\", onactivate:\"$.Msg('Button Pressed')\"" ).ToLocalChecked() );
				return;
			}
		}
	}

	args.GetReturnValue().Set( v8::Local<v8::Object>::New( GetV8Isolate(), *(UIEngineInternal()->CreateV8PanelInstance( pNewChild )) ) );
}


//-----------------------------------------------------------------------------
// Purpose: Handler for javascript DispatchEvent calls
//-----------------------------------------------------------------------------
extern void JSDispatchEvent( const v8::FunctionCallbackInfo<v8::Value>& args );
extern void JSDispatchEventAsync( const v8::FunctionCallbackInfo<v8::Value>& args );


//-----------------------------------------------------------------------------
// Purpose: $.Each() implementation
//-----------------------------------------------------------------------------
void JSEach( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if( args.Length() < 2 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Each requires 2 parameters, an object or array and a function to call on each element" ).ToLocalChecked() );
		return;
	}

	if( !args[0]->IsObject() && !args[0]->IsArray() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Invalid type for first param to Each() must be an array or object" ).ToLocalChecked() );
		return;
	}

	if( !args[1]->IsFunction() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Invalid type for second param to Each() must be a function" ).ToLocalChecked() );
		return;
	}

	v8::Handle<v8::Function> funcCallback = v8::Handle<v8::Function>::Cast( args[1] );
	v8::Handle<v8::Object> objGlobal = args.GetIsolate()->GetCurrentContext()->Global();
	v8::Local< v8::Context > context = args.GetIsolate()->GetCurrentContext();
	IUIPanel *pPanelContext = UIEngineInternal()->GetPanelForJavaScriptContext( *context );

	if( args[0]->IsObject() )
	{
		v8::Handle<v8::Object> obj = args[0]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		v8::Handle<v8::Array> keys = obj->GetPropertyNames( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Array>() );
		for( uint32_t i = 0; i < keys->Length(); ++i )
		{
			v8::Handle<v8::Value> rgParams[2];

			rgParams[0] = obj->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  keys->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
			rgParams[1] = keys->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() );
			
			v8::Handle<v8::Value> result = UIEngineInternal()->RunJSFunctionInternal( pPanelContext, context, objGlobal, funcCallback, 2, rgParams, false );
			
			if ( !result.IsEmpty() && result->IsBoolean() && result->BooleanValue( v8::Isolate::GetCurrent() ) == false )
				break;
		}
		args.GetReturnValue().Set( obj );
	}
	else if( args[0]->IsArray() )
	{
		v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast( args[0] );
		for( uint32_t i = 0; i < arr->Length(); ++i )
		{
			v8::Handle<v8::Value> rgParams[2];

			rgParams[0] = arr->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() );
			rgParams[1] = v8::Integer::NewFromUnsigned( args.GetIsolate(), i );

			v8::Handle<v8::Value> result = UIEngineInternal()->RunJSFunctionInternal( pPanelContext, context, objGlobal, funcCallback, 2, rgParams, false );

			if ( !result.IsEmpty() && result->IsBoolean() && result->BooleanValue( v8::Isolate::GetCurrent() ) == false )
				break;
		}
		args.GetReturnValue().Set( arr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: $.DbgIsReloadingScript() implementation
//-----------------------------------------------------------------------------
void JSDbgIsReloadingScript(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	bool bIsReloading = UIEngineInternal()->IsReloadingScript();
	args.GetReturnValue().Set( bIsReloading );
}

//-----------------------------------------------------------------------------
// Purpose: $.UrlEncode() implementation
//-----------------------------------------------------------------------------
void JSUrlEncode( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( args.Length() < 1 )
		args.GetReturnValue().Set( v8::String::NewFromUtf8( args.GetIsolate(), "" ).ToLocalChecked() );

	v8::String::Utf8Value value(v8::Isolate::GetCurrent(),  args[0] );
	const char* strValue = *value;
	if ( !strValue )
		return; // exception during value convert to string

	int len = value.length();
	if ( len > 2048 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "String too long for $.UrlEncode" ).ToLocalChecked() );
		return;
	}

	int buflen = len * 3 + 1;
	char *buf = (char*)stackalloc( buflen );
	if ( !buf )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Allocation failed" ).ToLocalChecked() );
		return;
	}

	Q_URLEncode( buf, buflen, strValue, len );
	args.GetReturnValue().Set( v8::String::NewFromUtf8( args.GetIsolate(), buf ).ToLocalChecked() );
}

//-----------------------------------------------------------------------------
// Purpose: $.UrlDecode() implementation
//-----------------------------------------------------------------------------
void JSUrlDecode( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( args.Length() < 1 )
		args.GetReturnValue().Set( v8::String::NewFromUtf8( args.GetIsolate(), "" ).ToLocalChecked() );

	v8::String::Utf8Value value(v8::Isolate::GetCurrent(),  args[0] );
	const char* strValue = *value;
	if ( !strValue )
		return; // exception during value convert to string

	int len = value.length();
	if ( len > 2048 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "String too long for $.UrlDecode" ).ToLocalChecked() );
		return;
	}

	int buflen = len + 1;
	char *buf = (char*)stackalloc( buflen );
	if ( !buf )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Allocation failed" ).ToLocalChecked() );
		return;
	}

	Q_URLDecode( buf, buflen, strValue, len );
	args.GetReturnValue().Set( v8::String::NewFromUtf8( args.GetIsolate(), buf ).ToLocalChecked() );
}

//-----------------------------------------------------------------------------
// Purpose: $.HTMLEscape() implementation
//-----------------------------------------------------------------------------
void JSHTMLEscape( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if ( args.Length() < 1 )
		args.GetReturnValue().Set( v8::String::NewFromUtf8( args.GetIsolate(), "" ).ToLocalChecked() );

	v8::String::Utf8Value value(v8::Isolate::GetCurrent(),  args[0] );
	const char* strValue = *value;
	if ( !strValue )
		return; // exception during value convert to string

	int dstLen;
	V_BasicHtmlEntityEncode( nullptr, 0, strValue, value.length(), &dstLen );

	const int kStackAllocMaxSize = 2048;
	char* dstBuf = ( char* )( ( dstLen > kStackAllocMaxSize ) ? malloc( dstLen ) : stackalloc( dstLen ) );
	if ( !dstBuf )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Allocation failed" ).ToLocalChecked() );
		return;
	}

	bool bSuccess = V_BasicHtmlEntityEncode( dstBuf, dstLen, strValue, value.length() );
	Assert( bSuccess );

	args.GetReturnValue().Set( v8::String::NewFromUtf8( args.GetIsolate(), dstBuf ).ToLocalChecked() );
	if ( dstLen > kStackAllocMaxSize )
		free( dstBuf );
}

//-----------------------------------------------------------------------------
// Purpose: Handler for javascript RegisterKeyBind calls
//-----------------------------------------------------------------------------
void JSRegisterKeyBind( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if( args.Length() < 3 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "RegisterKeyBind requires at least 3 params, panel or input context to bind on (optionally empty string for global bindings), key to bind (can be , delimited list), and binding event name or JS callback function" ).ToLocalChecked() );
		return;
	}

	if( !args[0]->IsString() && !args[0]->IsObject() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to RegisterKeyBind must be a panel object or string (input namespace, may be empty string)" ).ToLocalChecked() );
		return;
	}

	CUtlString strInputNamespace;
	IUIPanel *pPanelToRegisterOn = NULL;
	if( args[0]->IsString() )
	{
		v8::String::Utf8Value inputnamespace(v8::Isolate::GetCurrent(),  args[0] );
		strInputNamespace.Set( *inputnamespace );
	}
	else
	{
		v8::Local<v8::Object> obj = args[0]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		if( obj->InternalFieldCount() != 1 )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First argument to RegisterKeyBind must be a panel object or string (input namespace, may be empty string)" ).ToLocalChecked() );
			return;
		}

		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
		IUIPanel *pPanel = (IUIPanel*)wrap->Value();
		if( UIEngineInternal()->IsValidPanelPointer( pPanel ) )
		{
			pPanelToRegisterOn = pPanel;
		}
	}

	if( !args[1]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second argument to RegisterKeyBind is not a valid string (should be key name to bind)" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value keybinds(v8::Isolate::GetCurrent(),  args[1] );
	const char * pchKeyToBind = *keybinds;

	CUtlString strEventName;

	v8::Persistent<v8::Function> *pFunc = NULL;
	IUIPanel *pFuncContextPanel = NULL;
	char* pEvent = NULL;

	if( args[2]->IsFunction() )
	{
		v8::Handle<v8::Function> funcCallback = v8::Handle<v8::Function>::Cast( args[2] );

		pFunc = new v8::Persistent<v8::Function>();
		pFunc->Reset( args.GetIsolate(), funcCallback );

		pFuncContextPanel = UIEngine()->GetPanelForJavaScriptContext( *( args.GetIsolate()->GetCurrentContext() ) );
		if ( !pFuncContextPanel )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "RegisterKeyBind called outside panel context, can't register on a panel except in a containing panel/xml layout context" ).ToLocalChecked() );
			return;
		}
	}
	else if( args[2]->IsString() )
	{
		v8::String::Utf8Value eventname(v8::Isolate::GetCurrent(),  args[2] );
		strEventName = *eventname;

		if( !UIEngine()->IsValidEventName( strEventName.String() ) )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Third argument to RegisterKeyBind is not a valid callback function or event name" ).ToLocalChecked() );
			return;
		}

		pEvent = strEventName.Access();
	}
	else
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Third argument to RegisterKeyBind is not a valid callback function or event name" ).ToLocalChecked() );
		return;
	}

	CUtlVector<CUtlString> vecBinds;
	V_SplitString( pchKeyToBind, ",", vecBinds );

	FOR_EACH_VEC( vecBinds, i )
	{
		bool bSuccess = false;
		if( pPanelToRegisterOn )
			bSuccess = UIEngineInternal()->UIInputEngineInternal()->BRegisterKeyBind( pPanelToRegisterOn, vecBinds[i].String(), pEvent, pFunc, pFuncContextPanel );
		else
			bSuccess = UIEngineInternal()->UIInputEngineInternal()->BRegisterKeyBind( strInputNamespace.String(), vecBinds[i].String(), pEvent, pFunc, pFuncContextPanel );

		if( !bSuccess )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStr1024( "RegisterKeyBind failed, key %s to bind was probably invalid", vecBinds[i].String() ).String() ).ToLocalChecked() );
		}
	}

	if ( pFunc )
	{
		pFunc->Reset();
		delete pFunc;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tracking of inflight request helpers
//-----------------------------------------------------------------------------
void CUIEngine::TrackAsyncJSWebRequest( CJSAsyncWebRequest *pRequest )
{
	m_treeInFlightJSAsyncWebequestObjects.Insert( pRequest );
}


//-----------------------------------------------------------------------------
// Purpose: Tracking of inflight request helpers
//-----------------------------------------------------------------------------
void CUIEngine::ClearAsyncJSWebRequest( CJSAsyncWebRequest *pRequest )
{
	// We are shutting down, don't bother we'll clear the full tree in shutdown
	if ( g_IUIEngine == NULL )
		return;

	int iTree = m_treeInFlightJSAsyncWebequestObjects.Find( pRequest );
	if ( iTree != m_treeInFlightJSAsyncWebequestObjects.InvalidIndex() )
	{
		m_treeInFlightJSAsyncWebequestObjects.RemoveAt( iTree );
	}
	else
	{
		AssertMsg( false, "m_treeInFlightJSAsyncWebequestObjects missing load instance" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper job for async web api requests
//-----------------------------------------------------------------------------
CJSAsyncWebRequest::CJSAsyncWebRequest( HTTPRequestHandle hRequest, const char *pchURL, IUIPanel *ptrPanelContext,
	v8::Persistent<v8::Function> *pSuccessFunc, v8::Persistent<v8::Function> *pFailureFunc, v8::Persistent<v8::Function> *pCompleteFunc ) : m_fileResource( pchURL ), m_HTTPRequestCompleted( this, &CJSAsyncWebRequest::OnHTTPRequestCompleted )
{
	m_pPanelPtr = ptrPanelContext;
	m_pSuccessFunc = pSuccessFunc;
	m_pFailureFunc = pFailureFunc;
	m_pCompleteFunc = pCompleteFunc;
	m_hRequest = hRequest;

	UIEngineInternal()->TrackAsyncJSWebRequest( this );
	StartRequest();
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CJSAsyncWebRequest::~CJSAsyncWebRequest()
{
	if( m_pFailureFunc )
	{
		m_pFailureFunc->Reset();
		delete m_pFailureFunc;
	}

	if( m_pSuccessFunc )
	{
		m_pSuccessFunc->Reset();
		delete m_pSuccessFunc;
	}

	if( m_pCompleteFunc )
	{
		m_pCompleteFunc->Reset();
		delete m_pCompleteFunc;
	}

	if ( m_hRequest != INVALID_HTTPREQUEST_HANDLE )
	{
		ClientHTTP()->ReleaseHTTPRequest( m_hRequest );
		m_hRequest = INVALID_HTTPREQUEST_HANDLE;
	}
	
	UIEngineInternal()->ClearAsyncJSWebRequest( this );

}

const char * PchStringForEHTTPStatusCode( EHTTPStatusCode eStatus )
{
	switch( eStatus )
	{
		case k_EHTTPStatusCodeInvalid: 
			return "Invalid";
		case k_EHTTPStatusCode100Continue:
			return "Continue";
		case k_EHTTPStatusCode101SwitchingProtocols:
			return "Switching Protocols";
		case k_EHTTPStatusCode200OK:
			return "OK";
		case k_EHTTPStatusCode201Created:
			return "Created";
		case k_EHTTPStatusCode202Accepted:
			return "Accepted";
		case k_EHTTPStatusCode203NonAuthoritative:
			return "Non-Authoritative Information";
		case k_EHTTPStatusCode204NoContent:
			return "No Content";
		case k_EHTTPStatusCode205ResetContent:
			return "Reset Content";
		case k_EHTTPStatusCode206PartialContent:
			return "Partial Content";
		case k_EHTTPStatusCode300MultipleChoices:
			return "Multiple Choices";
		case k_EHTTPStatusCode301MovedPermanently:
			return "Moved Permanently";
		case k_EHTTPStatusCode302Found:
			return "Found";
		case k_EHTTPStatusCode303SeeOther:
			return "See Other";
		case k_EHTTPStatusCode304NotModified:
			return "Not Modified";
		case k_EHTTPStatusCode305UseProxy:
			return "Use Proxy";
		case k_EHTTPStatusCode307TemporaryRedirect:
			return "Temporary Redirect";
		case k_EHTTPStatusCode400BadRequest:
			return "Bad Request";
		case k_EHTTPStatusCode401Unauthorized:
			return "Unauthorized";
		case k_EHTTPStatusCode402PaymentRequired:
			return "Payment Required";
		case k_EHTTPStatusCode403Forbidden:
			return "Forbidden";
		case k_EHTTPStatusCode404NotFound:
			return "Not Found";
		case k_EHTTPStatusCode405MethodNotAllowed:
			return "Method Not Allowed";
		case k_EHTTPStatusCode406NotAcceptable:
			return "Not Acceptable";
		case k_EHTTPStatusCode407ProxyAuthRequired:
			return "Proxy Authentication Required";
		case k_EHTTPStatusCode408RequestTimeout:
			return "Request Timeout";
		case k_EHTTPStatusCode409Conflict:
			return "Conflict";
		case k_EHTTPStatusCode410Gone:
			return "Gone";
		case k_EHTTPStatusCode411LengthRequired:
			return "Length Required";
		case k_EHTTPStatusCode412PreconditionFailed:
			return "Precondition Failed";
		case k_EHTTPStatusCode413RequestEntityTooLarge:
			return "Request Entity Too Large";
		case k_EHTTPStatusCode414RequestURITooLong:
			return "Request-URI Too Large";
		case k_EHTTPStatusCode415UnsupportedMediaType:
			return "Unsupported Media Type";
		case k_EHTTPStatusCode416RequestedRangeNotSatisfiable:
			return "Requested range not satisfiable";
		case k_EHTTPStatusCode417ExpectationFailed:
			return "Expectation Failed";
		case k_EHTTPStatusCode4xxUnknown:
			return "Unknown HTTP 4xx";
		case k_EHTTPStatusCode429TooManyRequests:
			return "Too Many Requests";
		case k_EHTTPStatusCode500InternalServerError:
			return "Internal Server Error";
		case k_EHTTPStatusCode501NotImplemented:
			return "Not Implemented";
		case k_EHTTPStatusCode502BadGateway:
			return "Bad Gateway";
		case k_EHTTPStatusCode503ServiceUnavailable:
			return "Service Unavailable";
		case k_EHTTPStatusCode504GatewayTimeout:
			return "Gateway Time-out";
		case k_EHTTPStatusCode505HTTPVersionNotSupported:
			return "HTTP Version not supported";
		case k_EHTTPStatusCode5xxUnknown:
			return "Unknown HTTP 5xx";
		default:
			return "Unknown HTTP error";
	}
}

//-----------------------------------------------------------------------------
// Purpose: Job body
//-----------------------------------------------------------------------------
void CJSAsyncWebRequest::StartRequest()
{
	UIEngineInternal()->AddCommonHeadersToHttpRequest( m_hRequest );

	const CUtlVector<CUtlString> &vecCookies = m_fileResource.GetCookieHeadersForHTTPURL();
	if( vecCookies.Count() )
	{
		char rgchDomain[1024];
		V_ExtractDomainFromURL( m_fileResource.GetReferencePath(), rgchDomain, V_ARRAYSIZE( rgchDomain ) );

		HTTPCookieContainerHandle hCookieContainer = UIEngineInternal()->GetCookieContainerForDomain( rgchDomain );
		FOR_EACH_VEC( vecCookies, iCookie )
		{
			ClientHTTP()->SetCookie( hCookieContainer, rgchDomain, "/", vecCookies[iCookie].String() );
		}

		ClientHTTP()->SetHTTPRequestCookieContainer( m_hRequest, hCookieContainer );
	}

	// for https requests, we always want to verify the server cert
	if( StringHasPrefix( m_fileResource.GetReferencePath(), "https" ) )
		ClientHTTP()->SetHTTPRequestRequiresVerifiedCertificate( m_hRequest, true );

	SteamAPICall_t hSteamAPICall;
	if ( ClientHTTP()->SendHTTPRequest( m_hRequest, &hSteamAPICall ) )
	{
		// Add call handle to get callback
		m_HTTPRequestCompleted.AddCall( hSteamAPICall );
	}
	else
	{
		// Not really ready, but this handles failure too
		HTTPRequestCompleted_t callback;
		callback.m_bRequestSuccessful = false;
		callback.m_hRequest = m_hRequest;
		callback.m_eStatusCode = k_EHTTPStatusCode500InternalServerError;
		OnHTTPRequestCompleted( &callback, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Got response or failure
//-----------------------------------------------------------------------------
void CJSAsyncWebRequest::OnHTTPRequestCompleted( HTTPRequestCompleted_t *pParam, bool bIOFailure )
{
	bool bSuccess = pParam != NULL && !bIOFailure;
	if ( pParam )
		Assert( pParam->m_hRequest == m_hRequest );

	// If the panel js context is gone, then we can't execute any callbacks, so we are done
	if( m_pPanelPtr.Get() )
	{
		v8::Isolate::Scope isolate_scope( UIEngineInternal()->GetV8Isolate() );
		v8::HandleScope handle_scope( UIEngineInternal()->GetV8Isolate() );

		v8::Persistent<v8::Context> *pContextRef = UIEngineInternal()->GetContextForPanel( m_pPanelPtr.Get() );
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( UIEngineInternal()->GetV8Isolate(), *pContextRef );

		v8::Context::Scope context_scope( context );
		v8::TryCatch try_catch( v8::Isolate::GetCurrent() );

		v8::Handle<v8::Object> obj = context->Global();

		const char *pchStatus = "error";

		if( bSuccess )
		{
			if( (pParam->m_eStatusCode == k_EHTTPStatusCode200OK || pParam->m_eStatusCode == k_EHTTPStatusCode304NotModified) )
			{
				if( pParam->m_eStatusCode == k_EHTTPStatusCode304NotModified )
					pchStatus = "notmodified";
				else
					pchStatus = "success";

				if( m_pSuccessFunc )
				{
					// Call JS success function
					v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), *(m_pSuccessFunc) );

					v8::Handle<v8::Value> arguments[3];
					bool bConvertedJSON = false;

					char rgchType[1024]; 
					if ( ClientHTTP()->GetHTTPResponseHeaderValue( m_hRequest, "content-type", (uint8*)rgchType, 1024 ) && V_stristr( rgchType, "application/json" ) != NULL )
					{
						// Looks like application/json, we ignore charset, but we should parse and set as data param.
						VPROF_BUDGET( "CJobJSAsyncWebRequest - parse JSON to V8 object", VPROF_BUDGETGROUP_TENFOOT );

						bConvertedJSON = true;

						uint32 unBodySize = 0;
						ClientHTTP()->GetHTTPResponseBodySize( m_hRequest, &unBodySize );
						CUtlBuffer bufBody;
						bufBody.EnsureCapacity( unBodySize );

						if ( ClientHTTP()->GetHTTPResponseBodyData( m_hRequest, (uint8*)bufBody.Base(), unBodySize ) )
						{
							bufBody.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );

							v8::Handle<v8::String> strJSON = v8::String::NewFromUtf8( UIEngineInternal()->GetV8Isolate(), (const char*)bufBody.Base(), v8::NewStringType::kNormal, unBodySize ).ToLocalChecked();
							v8::Handle<v8::Value> returnval = v8::JSON::Parse( v8::Isolate::GetCurrent()->GetCurrentContext(), strJSON ).FromMaybe( v8::Local<v8::Value>() );

							if( returnval.IsEmpty() )
							{
								// try_catch from above is still active and will have caught an exception
								UIEngineInternal()->OutputJSString( m_pPanelPtr.Get(), CFmtStr1024( "!! Exception trying to parse JSON in ajax response for %s", m_fileResource.GetReferencePath().String() ).String() );
								arguments[0] = v8::Null( UIEngineInternal()->GetV8Isolate() );
							}
							else
							{
								arguments[0] = returnval;
							}
						}
						else
						{
							// try_catch from above is still active and will have caught an exception
							UIEngineInternal()->OutputJSString( m_pPanelPtr.Get(), CFmtStr1024( "!! Exception trying to get body JSON in ajax response for %s", m_fileResource.GetReferencePath().String() ).String() );
							arguments[0] = v8::Null( UIEngineInternal()->GetV8Isolate() );
						}
					}

					if( !bConvertedJSON )
					{
						uint32 unBodySize = 0;
						ClientHTTP()->GetHTTPResponseBodySize( m_hRequest, &unBodySize );
						CUtlBuffer bufBody;
						bufBody.EnsureCapacity( unBodySize );

						if ( ClientHTTP()->GetHTTPResponseBodyData( m_hRequest, (uint8*)bufBody.Base(), unBodySize ) )
						{
							bufBody.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
							bufBody.PutChar( '\0' );

							const char *pchBody = (const char*)bufBody.Base();
							PanoramaTypeToV8Param( pchBody, &arguments[0] );
						}
						else
						{
							// try_catch from above is still active and will have caught an exception
							UIEngineInternal()->OutputJSString( m_pPanelPtr.Get(), CFmtStr1024( "!! Exception trying to get body in ajax response for %s", m_fileResource.GetReferencePath().String() ).String() );
							arguments[0] = v8::Null( UIEngineInternal()->GetV8Isolate() );
						}
					}

					PanoramaTypeToV8Param( (const char*)pchStatus, &arguments[1] );
					PanoramaTypeToV8Param( PchStringForEHTTPStatusCode( pParam->m_eStatusCode ), &arguments[2] );

					VPROF_BUDGET( "$.AsyncWebRequest - success callback", VPROF_BUDGETGROUP_TENFOOT );

					UIEngineInternal()->PushContextPanel( m_pPanelPtr.Get() );
					fnLocal->Call( v8::Isolate::GetCurrent()->GetCurrentContext(),  obj, 3, arguments ).FromMaybe( v8::Local<v8::Value>() );
					UIEngineInternal()->PopContextPanel();
				}
			}
			else
			{
				pchStatus = "error";
				if( m_pFailureFunc )
				{
					v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), *(m_pFailureFunc) );

					v8::Handle<v8::Value> arguments[3];
					XMLHttpRequestObjectFromHTTPResponse( pParam, pchStatus, &arguments[0] );
					if ( pParam )
					{
						PanoramaTypeToV8Param( pchStatus, &arguments[1] );
						PanoramaTypeToV8Param( PchStringForEHTTPStatusCode( pParam->m_eStatusCode ), &arguments[2] );
					}
					else
					{
						PanoramaTypeToV8Param( pchStatus, &arguments[1] );
						PanoramaTypeToV8Param( "", &arguments[2] );
					}

					VPROF_BUDGET( "$.AsyncWebRequest - failure callback", VPROF_BUDGETGROUP_TENFOOT );

					UIEngineInternal()->PushContextPanel( m_pPanelPtr.Get() );
					fnLocal->Call( v8::Isolate::GetCurrent()->GetCurrentContext(),  obj, 3, arguments ).FromMaybe( v8::Local<v8::Value>() );
					UIEngineInternal()->PopContextPanel();
				}
			}
		}
		else 
		{
			bool bWasRequestTimedOut = false;
			ClientHTTP()->GetHTTPRequestWasTimedOut( m_hRequest, &bWasRequestTimedOut );

			if( bWasRequestTimedOut )
				pchStatus = "timeout";
			else
				pchStatus = "abort";

			if( m_pFailureFunc )
			{
				v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), *(m_pFailureFunc) );

				v8::Handle<v8::Value> arguments[3];

				XMLHttpRequestObjectFromHTTPResponse( NULL, pchStatus, &arguments[0] );
				PanoramaTypeToV8Param( pchStatus, &arguments[1] );
				PanoramaTypeToV8Param( "", &arguments[2] );
				VPROF_BUDGET( "$.AsyncWebRequest - failure callback", VPROF_BUDGETGROUP_TENFOOT );

				UIEngineInternal()->PushContextPanel( m_pPanelPtr.Get() );
				fnLocal->Call( v8::Isolate::GetCurrent()->GetCurrentContext(),  obj, 3, arguments ).FromMaybe( v8::Local<v8::Value>() );
				UIEngineInternal()->PopContextPanel();
			}
		}

		// Output any exception we caught
		if( try_catch.HasCaught() )
		{
			UIEngineInternal()->OutputJSExceptionToConsole( try_catch, m_pPanelPtr.Get() );
		}

		try_catch.Reset();


		// If we had a complete func call it last after success/failure callbacks
		if( m_pCompleteFunc )
		{
			// Call JS success function
			v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), *(m_pCompleteFunc) );

			v8::Handle<v8::Value> arguments[2];
			XMLHttpRequestObjectFromHTTPResponse( pParam, pchStatus, &arguments[0] );
			PanoramaTypeToV8Param( pchStatus, &arguments[1] );

			VPROF_BUDGET( "$.AsyncWebRequest - complete callback", VPROF_BUDGETGROUP_TENFOOT );
			
			UIEngineInternal()->PushContextPanel( m_pPanelPtr.Get() );
			fnLocal->Call( v8::Isolate::GetCurrent()->GetCurrentContext(),  obj, 2, arguments ).FromMaybe( v8::Local<v8::Value>() );
			UIEngineInternal()->PopContextPanel();
		}

		if( try_catch.HasCaught() )
		{
			UIEngineInternal()->OutputJSExceptionToConsole( try_catch, m_pPanelPtr.Get() );
		}
	}

	delete this;
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CJSAsyncWebRequest::Validate( CValidator &validator, const char *pchName )
{
	ValidateObj( m_fileResource );
	ValidateObj( m_HTTPRequestCompleted );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Create an XHR object similar to jQuery's for use in callbacks.
//-----------------------------------------------------------------------------
void CJSAsyncWebRequest::XMLHttpRequestObjectFromHTTPResponse( HTTPRequestCompleted_t *pParam, const char *pchStatus, v8::Handle<v8::Value> *pXHROut )
{
	v8::Handle<v8::Object> xhr = v8::Object::New( GetV8Isolate() );
	xhr->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "statusText" ).ToLocalChecked(), v8::String::NewFromUtf8( GetV8Isolate(), pchStatus ).ToLocalChecked() );

	if ( pParam )
	{
		uint32 unBodySize = 0;
		ClientHTTP()->GetHTTPResponseBodySize( pParam->m_hRequest, &unBodySize );
		CUtlBuffer bufBody;
		bufBody.EnsureCapacity( unBodySize );

		if ( ClientHTTP()->GetHTTPResponseBodyData( pParam->m_hRequest, (uint8*)bufBody.Base(), unBodySize ) )
		{
			bufBody.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
			bufBody.PutChar( '\0' );

			const char *pchBody = (const char*)bufBody.Base();

			xhr->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "responseText" ).ToLocalChecked(), v8::String::NewFromUtf8( GetV8Isolate(), pchBody, v8::NewStringType::kNormal, bufBody.TellPut() ).ToLocalChecked() );
		}
		else
		{
			xhr->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "responseText" ).ToLocalChecked(), v8::Null( GetV8Isolate() ) );
		}
		xhr->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "status" ).ToLocalChecked(), v8::Integer::New( GetV8Isolate(), pParam->m_eStatusCode ) );
	}
	else
	{
		xhr->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "responseText" ).ToLocalChecked(), v8::Null( GetV8Isolate() ) );
		xhr->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "status" ).ToLocalChecked(), v8::Null( GetV8Isolate() ) );
	}

	*pXHROut = xhr;
}

//-----------------------------------------------------------------------------
// Purpose: Recursive helper to set GET/POST params for JSAsyncWebRequest
//-----------------------------------------------------------------------------
void SetJSWebRequestParams( const v8::FunctionCallbackInfo<v8::Value>& args, HTTPRequestHandle hRequest, v8::Handle<v8::Value> valData, const char* pchKey )
{
	// Functions are also objects, explicitly exclude them else we run into some unexpected behavior
	if ( valData->IsObject() && !valData->IsFunction() )
	{
		v8::Local<v8::Object> dataObj = valData->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		v8::Local<v8::Array> dataNames = dataObj->GetPropertyNames( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Array>() );

		for ( uint32_t i = 0; i < dataNames->Length(); ++i )
		{
			v8::String::Utf8Value paramName(v8::Isolate::GetCurrent(),  dataNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) );
			if ( *paramName )
			{
				v8::Handle<v8::Value> paramValue = dataObj->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  dataNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );

				// If we have a key already, make sure we're using the subkey syntax
				if ( V_strlen( pchKey ) > 0 )
				{
					const char* pchSubKey = CFmtStr1024( "%s[%s]", pchKey, *paramName ).String();
					SetJSWebRequestParams( args, hRequest, paramValue, pchSubKey );
				} 
				else
					SetJSWebRequestParams( args, hRequest, paramValue, *paramName );
			}
		}
	}
	else if ( valData->IsArray() )
	{

		// Arrays require special handling. We have upstream code that prevents duplicate POST params
		// so we'll use the Param[index] = Value format.
		v8::Handle<v8::Array> paramArray = v8::Handle<v8::Array>::Cast( valData );
		for ( uint32_t j = 0; j < paramArray->Length(); j++ )
		{
			v8::Local<v8::Value> paramArrayValue = paramArray->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  j ).FromMaybe( v8::Local<v8::Value>() );

			const char* pchSubKey = CFmtStr1024( "%s[%i]", pchKey, j ).String();
			SetJSWebRequestParams( args, hRequest, paramArrayValue, pchSubKey );

		}
		
	}
	else // For all other types, try to slam it to a string
	{
		v8::String::Utf8Value paramString(v8::Isolate::GetCurrent(),  valData );

		if ( *paramString )
		{
			ClientHTTP()->SetHTTPRequestGetOrPostParameter( hRequest, pchKey, *paramString );
		}
		else
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "values for keys in data value for AsyncWebRequest settings must be an object, array, or convertaile to a string" ).ToLocalChecked() );
			ClientHTTP()->ReleaseHTTPRequest( hRequest );
			return;
		}
	}


}

//-----------------------------------------------------------------------------
// Purpose: Handler for javascript AsyncWebRequest calls
//-----------------------------------------------------------------------------
void JSAsyncWebRequest( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	if( args.Length() < 2 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "AsyncWebRequest requires 2 params, url and settings key->value object" ).ToLocalChecked() );
		return;
	}

	v8::String::Utf8Value url(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchURL = *url;
	if( !pchURL )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "First param to AsyncWebRequest must be URL" ).ToLocalChecked() );
		return;
	}

	if( !args[1]->IsObject() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Second param to AsyncWebRequest must be object of named key=>value settings" ).ToLocalChecked() );
		return;
	}

	if ( !ClientHTTP() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "AsyncWebRequest requires steam to be running" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pContext = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	EHTTPMethod eMethod = k_EHTTPMethodGET;
	float flTimeoutMilliseconds = 60000;

	v8::Handle<v8::Function> successCallback;
	v8::Handle<v8::Function> failureCallback;
	v8::Handle<v8::Function> completeCallback;

	v8::Handle<v8::Value> valHeaders;
	v8::Handle<v8::Value> valData;

	v8::Local<v8::Object> objSettings = args[1]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
	v8::Local<v8::Array> settingNames = objSettings->GetPropertyNames( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Array>() );

	for( uint32_t i = 0; i < settingNames->Length(); ++i )
	{
		v8::String::Utf8Value settingName(v8::Isolate::GetCurrent(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) );
		if( *settingName )
		{
			if( V_stricmp( *settingName, "type" ) == 0 )
			{
				v8::Handle<v8::Value> settingValue = objSettings->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
				v8::String::Utf8Value settingString(v8::Isolate::GetCurrent(),  settingValue );
				if( *settingString )
				{
					if( V_stricmp( *settingString, "GET" ) == 0 )
						eMethod = k_EHTTPMethodGET;
					else if( V_stricmp( *settingString, "POST" ) == 0 )
						eMethod = k_EHTTPMethodPOST;
					else if( V_stricmp( *settingString, "PUT" ) == 0 )
						eMethod = k_EHTTPMethodPUT;
					else if( V_stricmp( *settingString, "DELETE" ) == 0 )
						eMethod = k_EHTTPMethodDELETE;
					else if( V_stricmp( *settingString, "HEAD" ) == 0 )
						eMethod = k_EHTTPMethodHEAD;
					else
					{
						// Don't want/need an exception here, but do want some warning...
						UIEngineInternal()->OutputJSString( pContext, CFmtStr1024( "!! Warning: Invalid 'type' param %s to AsyncWebRequest, defaulting to GET", *settingString ).String() );
					}
				}
				else
				{
					// Don't want/need an exception here, but do want some warning...
					UIEngineInternal()->OutputJSString( pContext, CFmtStr1024( "!! Warning: Invalid 'type' param undefined to AsyncWebRequest, defaulting to GET" ).String() );
				}
			}
			else if( V_stricmp( *settingName, "timeout" ) == 0 )
			{
				v8::Handle<v8::Value> settingValue = objSettings->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
				v8::Handle<v8::Number> timeoutValue = settingValue->ToNumber( GetV8Isolate()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Number>() );
				if( !timeoutValue.IsEmpty() )
					flTimeoutMilliseconds = (float)timeoutValue->Value();
				else
				{
					// Don't want/need an exception here, but do want some warning...
					UIEngineInternal()->OutputJSString( pContext, CFmtStr1024( "!! Warning: Invalid 'timeout' param (not a number) to AsyncWebRequest, using default timeout" ).String() );
				}
			}
			else if( V_stricmp( *settingName, "error" ) == 0 )
			{
				v8::Handle<v8::Value> settingValue = objSettings->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
				if( settingValue->IsFunction() )
				{
					failureCallback = v8::Handle<v8::Function>::Cast( settingValue );
				}
				else
				{
					args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "error value for AsyncWebRequest settings must be a function if set" ).ToLocalChecked() );
					return;
				}
			}
			else if( V_stricmp( *settingName, "success" ) == 0 )
			{
				v8::Handle<v8::Value> settingValue = objSettings->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
				if( settingValue->IsFunction() )
				{
					successCallback = v8::Handle<v8::Function>::Cast( settingValue );
				}
				else
				{
					args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "error value for AsyncWebRequest settings must be a function if set" ).ToLocalChecked() );
					return;
				}
			}
			else if( V_stricmp( *settingName, "complete" ) == 0 )
			{
				v8::Handle<v8::Value> settingValue = objSettings->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
				if( settingValue->IsFunction() )
				{
					completeCallback = v8::Handle<v8::Function>::Cast( settingValue );
				}
				else
				{
					args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "complete value for AsyncWebRequest settings must be a function if set" ).ToLocalChecked() );
					return;
				}
			}
			else if( V_stricmp( *settingName, "data" ) == 0 )
			{
				valData = objSettings->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
			}
			else if( V_stricmp( *settingName, "headers" ) == 0 )
			{
				valHeaders = objSettings->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  settingNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
			}
		}
	}

	HTTPRequestHandle hRequest = ClientHTTP()->CreateHTTPRequest( eMethod, pchURL );
	ClientHTTP()->SetHTTPRequestAbsoluteTimeoutMS( hRequest, (uint32)flTimeoutMilliseconds );

	if( !valHeaders.IsEmpty() )
	{
		if( !valHeaders->IsObject() )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "header value for AsyncWebRequest settings must be an object with named key->value headers if set" ).ToLocalChecked() );
			ClientHTTP()->ReleaseHTTPRequest( hRequest );
			return;
		}
		else
		{
			v8::Local<v8::Object> headerObj = valHeaders->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
			v8::Local<v8::Array> headerNames = headerObj->GetPropertyNames( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Array>() );

			for( uint32_t i = 0; i < headerNames->Length(); ++i )
			{
				v8::String::Utf8Value headerName(v8::Isolate::GetCurrent(),  headerNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) );
				if( *headerName )
				{
					v8::Handle<v8::Value> headerValue = headerObj->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  headerNames->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  i ).FromMaybe( v8::Local<v8::Value>() ) ).FromMaybe( v8::Local<v8::Value>() );
					v8::String::Utf8Value headerString(v8::Isolate::GetCurrent(),  headerValue );

					if( *headerString )
					{
						ClientHTTP()->SetHTTPRequestHeaderValue( hRequest, *headerName, *headerString );
					}
					else
					{
						args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "values for keys in header value for AsyncWebRequest settings must be convertible to strings" ).ToLocalChecked() );
						ClientHTTP()->ReleaseHTTPRequest( hRequest );
						return;
					}
				}
			}
		}
	}

	if( !valData.IsEmpty() )
	{
		if( !valData->IsObject() )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "data value for AsyncWebRequest settings must be an object with named key->value request params if set" ).ToLocalChecked() );
			ClientHTTP()->ReleaseHTTPRequest( hRequest );
			return;
		}
		else
		{
			SetJSWebRequestParams( args, hRequest, valData, "" );
		}
	}

	bool bAllowRequest = UIEngine()->BMatchDomainForJSRequest( pContext, pchURL );	
	if( !bAllowRequest )
	{
		const char *pchLayout = pContext->GetLayoutFilePathForJSCheck();
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), CFmtStrN<4096>( "AsyncWebRequest attempted across domain (layout:%s url:%s) blocked.", pchLayout, pchURL ).String() ).ToLocalChecked() );
		ClientHTTP()->ReleaseHTTPRequest( hRequest );
		return;
	}

	v8::Persistent<v8::Function> *pSuccessCallback = NULL;
	if( !successCallback.IsEmpty() )
	{
		pSuccessCallback = new v8::Persistent<v8::Function>( args.GetIsolate(), successCallback );
	}

	v8::Persistent<v8::Function> *pFailureCallback = NULL;
	if( !failureCallback.IsEmpty() )
	{
		pFailureCallback = new v8::Persistent<v8::Function>( args.GetIsolate(), failureCallback );
	}

	v8::Persistent<v8::Function> *pCompleteCallback = NULL;
	if( !completeCallback.IsEmpty() )
	{
		pCompleteCallback = new v8::Persistent<v8::Function>( args.GetIsolate(), completeCallback );
	}


	new CJSAsyncWebRequest( hRequest, pchURL, pContext, pSuccessCallback, pFailureCallback, pCompleteCallback );
}


//-----------------------------------------------------------------------------
// Purpose: Handler for javascript FindChildInContext calls
//-----------------------------------------------------------------------------
void JSFindChildInContext( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	IUIPanel *pContext = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	if( !pContext || args.Length() < 1 )
	{
		args.GetReturnValue().Set( v8::Null( args.GetIsolate() ) );
		return;
	}

	v8::String::Utf8Value id(v8::Isolate::GetCurrent(),  args[0] );
	const char * pchPanelID = *id;

	if( !pchPanelID  )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Should not pass empty string for FindChildInContext()/$()" ).ToLocalChecked() );
		return;
	}

	if( pchPanelID[0] != '#' )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "FindChildInContext()/$() require an id selector (start with #) for now" ).ToLocalChecked() );
		return;
	}

	pchPanelID = pchPanelID + 1;

	IUIPanel *pPanel = NULL;
	if ( V_strcmp( pContext->GetID(), pchPanelID ) == 0 )
	{
		pPanel = pContext;
	}
	else
	{
		pPanel = pContext->FindChildInLayoutFile( pchPanelID );

		// try other panels associated with context
		if ( !pPanel )
		{
			CUtlVector< IUIPanel * > *pvecAssociated = UIEngineInternal()->GetAssociatedPanelsForV8Context( pContext );
			if ( pvecAssociated )
			{
				FOR_EACH_VEC( *pvecAssociated, i )
				{
					IUIPanel *pAssociated = pvecAssociated->Element( i );
					if ( V_strcmp( pAssociated->GetID(), pchPanelID ) == 0 )
					{
						pPanel = pAssociated;
						break;
					}

					pPanel = pAssociated->FindChildInLayoutFile( pchPanelID );
					if ( pPanel )
						break;
				}
			}
		}
	}

	if( !pPanel )
	{
		args.GetReturnValue().Set( v8::Null( args.GetIsolate() ) );
		return;
	}
	
	args.GetReturnValue().Set( v8::Local<v8::Object>::New( GetV8Isolate(), *(UIEngineInternal()->CreateV8PanelInstance( pPanel ) ) ) );

	return;
}


//-----------------------------------------------------------------------------
// Purpose: JS $ shortcut helper method
//-----------------------------------------------------------------------------
void JSDollarSign( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	JSFindChildInContext( args );
}


//-----------------------------------------------------------------------------
// Purpose: Is the object type name already exposed to JavaScript?
//-----------------------------------------------------------------------------
bool CUIEngine::IsObjectTypeExposedToJavaScript( const char *pchObjectTypeName )
{
	if ( m_mapV8ClassTemplatesByType.Find( pchObjectTypeName ) != m_mapV8ClassTemplatesByType.InvalidIndex() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Expose a new object type/template to javascript with the given name, 
// the function pointer passed should setup member accessors/methods with the functions
// from uijsregistration.h
//-----------------------------------------------------------------------------
void CUIEngine::ExposeObjectTypeToJavaScript( const char *pchObjectTypeName, CUtlAbstractDelegate &del )
{
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, m_V8UIEngineGlobalContext );
	v8::Context::Scope context_scope( context );

	Assert( m_v8ClassTemplateSetupCur.IsEmpty() );
	m_v8ClassTemplateSetupCur = v8::FunctionTemplate::New( m_pV8Isolate );
	m_v8ClassTemplateSetupCur->InstanceTemplate()->SetInternalFieldCount( 1 );
	m_v8ClassTemplateSetupCur->SetClassName( v8::String::NewFromUtf8( m_pV8Isolate, pchObjectTypeName ).ToLocalChecked() );

	Assert( m_v8ClassSignatureSetupCur.IsEmpty() );
	m_v8ClassSignatureSetupCur = v8::Signature::New( m_pV8Isolate, m_v8ClassTemplateSetupCur );

	Assert( m_v8ClassAccessorSignatureSetupCur.IsEmpty() );
	m_v8ClassAccessorSignatureSetupCur = v8::AccessorSignature::New( m_pV8Isolate, m_v8ClassTemplateSetupCur );

	CUtlDelegate< void() > ldel;
	ldel.SetAbstractDelegate( del );
	ldel();

	v8::Persistent<v8::FunctionTemplate> *pTemplate = new v8::Persistent<v8::FunctionTemplate>();
	pTemplate->Reset( m_pV8Isolate, m_v8ClassTemplateSetupCur );

	m_v8ClassTemplateSetupCur.Clear();
	m_v8ClassSignatureSetupCur.Clear();
	m_v8ClassAccessorSignatureSetupCur.Clear();

	m_mapV8ClassTemplatesByType.Insert( pchObjectTypeName, pTemplate );
}


//-----------------------------------------------------------------------------
// Purpose: Clear the global object in JS for an object we are deleting in C++
//-----------------------------------------------------------------------------
void CUIEngine::ClearGlobalObjectForJavaScript( const char *pchJSVarName, void *pInstance )
{
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	if( !m_V8UIEngineGlobalContext.IsEmpty() )
	{
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, m_V8UIEngineGlobalContext );
		v8::Context::Scope context_scope( context );

		v8::Local<v8::Object> obj = context->Global()->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() )->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		obj->SetInternalField( 0, v8::External::New( m_pV8Isolate, 0 ) );
		context->Global()->Delete( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked() );
	}

	FOR_EACH_MAP_FAST( m_MapPanelV8Contexts, i )
	{
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *(m_MapPanelV8Contexts[i]) );
		v8::Context::Scope context_scope( context );

		v8::Local<v8::Object> obj = context->Global()->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() )->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		obj->SetInternalField( 0, v8::External::New( m_pV8Isolate, 0 ) );
		context->Global()->Delete( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked() );
	}


	// Also stop creating instances in new contexts, find by name to do so
	FOR_EACH_VEC_BACK( m_vecV8GlobalObjectRegistrations, i )
	{
		if( m_vecV8GlobalObjectRegistrations[i].m_strName == pchJSVarName )
		{
			m_vecV8GlobalObjectRegistrations[i].m_pObj->Reset();
			delete m_vecV8GlobalObjectRegistrations[i].m_pObj;
			m_vecV8GlobalObjectRegistrations.Remove( i );
			break;
		}
	}

	v8::Handle<v8::ObjectTemplate> global = v8::Local<v8::ObjectTemplate>::New( m_pV8Isolate, m_V8GlobalTemplate );
	global->Set( v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked(), v8::Null( m_pV8Isolate ) );
	m_V8GlobalTemplate.Reset( m_pV8Isolate, global );
}


//-----------------------------------------------------------------------------
// Purpose: Expose a global function to javascript
//-----------------------------------------------------------------------------
void CUIEngine::AddGlobalV8FunctionTemplate( const char *pchJSFuncName, v8::Handle< v8::FunctionTemplate > *pFunc, bool bTrueGlobal )
{
	// Create a template for the global object.
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, m_V8UIEngineGlobalContext );
	v8::Context::Scope context_scope( context );

	if( !m_V8UIEngineGlobalContext.IsEmpty() )
	{
		v8::Handle<v8::Context> contextinner = v8::Local<v8::Context>::New( m_pV8Isolate, m_V8UIEngineGlobalContext );
		if( !bTrueGlobal )
		{
			v8::Handle<v8::Value> p = contextinner->Global()->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  v8::String::NewFromUtf8( m_pV8Isolate, "panorama" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if( p->IsObject() )
				p->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() )->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSFuncName ).ToLocalChecked(), (*pFunc)->GetFunction( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Function>() ) );
			else
				Assert( false );
		}
		else
		{
			contextinner->Global()->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSFuncName ).ToLocalChecked(), (*pFunc)->GetFunction( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Function>() ) );
		}
	}

	FOR_EACH_MAP_FAST( m_MapPanelV8Contexts, i )
	{
		v8::Handle<v8::Context> contextinner = v8::Local<v8::Context>::New( m_pV8Isolate, *(m_MapPanelV8Contexts[i]) );
		if( !bTrueGlobal )
		{
			v8::Handle<v8::Value> p = contextinner->Global()->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  v8::String::NewFromUtf8( m_pV8Isolate, "panorama" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if( p->IsObject() )
				p->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() )->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSFuncName ).ToLocalChecked(), (*pFunc)->GetFunction( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Function>() ) );
			else
				Assert( false );
		}
		else
		{
			contextinner->Global()->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSFuncName ).ToLocalChecked(), (*pFunc)->GetFunction( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Function>() ) );
		}
	}
	
	int iVec = m_vecV8GlobalFunctionRegistrations.AddToTail();
	V8GlobalFunctionRegistration_t &reg = m_vecV8GlobalFunctionRegistrations[iVec];
	reg.m_bTrueGlobal = bTrueGlobal;
	reg.m_pFunction = new v8::Persistent<v8::FunctionTemplate>();
	reg.m_pFunction->Reset( m_pV8Isolate, *pFunc );
	reg.m_strName = pchJSFuncName;

	m_V8PanoramaTemplate.Reset();
	m_V8GlobalTemplate.Reset();
}


//-----------------------------------------------------------------------------
// Purpose: Expose an instance of an object type as a global with specified name to javascript
//-----------------------------------------------------------------------------
void CUIEngine::ExposeGlobalObjectToJavaScript( const char *pchJSVarName, void *pInstance, const char *pchJsTypeName, bool bTrueGlobal )
{
	// Create a template for the global object.
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, m_V8UIEngineGlobalContext );
	v8::Context::Scope context_scope( context );

	int iMap = m_mapV8ClassTemplatesByType.Find( pchJsTypeName );
	if( m_mapV8ClassTemplatesByType.InvalidIndex() == iMap )
	{
		AssertMsg1( false, "Object type %s is not registered", pchJsTypeName );
		return;
	}

	v8::Persistent<v8::FunctionTemplate> *pTemplate = m_mapV8ClassTemplatesByType[iMap];
	v8::Handle<v8::FunctionTemplate> classTemplate = v8::Local<v8::FunctionTemplate>::New( m_pV8Isolate, *pTemplate );

	v8::Local<v8::Object> obj = classTemplate->InstanceTemplate()->NewInstance( context ).ToLocalChecked();

	obj->SetInternalField( 0, v8::External::New( m_pV8Isolate, pInstance ) );

	v8::Persistent<v8::Object> *persistentObject = new v8::Persistent<v8::Object>();
	persistentObject->Reset( m_pV8Isolate, obj );

	
	m_MapV8GlobalObjectInstances.Insert( pInstance, persistentObject );
	if( !m_V8UIEngineGlobalContext.IsEmpty() )
	{
		v8::Handle<v8::Context> contextinner = v8::Local<v8::Context>::New( m_pV8Isolate, m_V8UIEngineGlobalContext );
		if( !bTrueGlobal )
		{
			v8::Handle<v8::Value> p = contextinner->Global()->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  v8::String::NewFromUtf8( m_pV8Isolate, "panorama" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if( p->IsObject() )
				p->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() )->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked(), obj );
			else
				Assert( false );
		}
		else
		{
			contextinner->Global()->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked(), obj );
		}
	}

	FOR_EACH_MAP_FAST( m_MapPanelV8Contexts, i )
	{
		v8::Handle<v8::Context> contextinner = v8::Local<v8::Context>::New( m_pV8Isolate, *(m_MapPanelV8Contexts[i]) );
		if( !bTrueGlobal )
		{
			v8::Handle<v8::Value> p = contextinner->Global()->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  v8::String::NewFromUtf8( m_pV8Isolate, "panorama" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if( p->IsObject() )
				p->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() )->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked(), obj );
			else
				Assert( false );
		}
		else
		{
			contextinner->Global()->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, pchJSVarName ).ToLocalChecked(), obj );
		}
	}
	
	int iVec = m_vecV8GlobalObjectRegistrations.AddToTail();
	V8GlobalObjectRegistration_t &reg = m_vecV8GlobalObjectRegistrations[iVec];
	reg.m_bTrueGlobal = bTrueGlobal;
	reg.m_pObj = new v8::Persistent<v8::Object>();
	reg.m_pObj->Reset( m_pV8Isolate, obj );
	reg.m_strName = pchJSVarName;

	m_V8PanoramaTemplate.Reset();
	m_V8GlobalTemplate.Reset();
}


//-----------------------------------------------------------------------------
// Purpose: Find object template to use for returning given panel type to js as an object
//-----------------------------------------------------------------------------
v8::Persistent<v8::FunctionTemplate> *CUIEngine::GetJSClassTemplateForPanel( IUIPanel *pPanel )
{
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	CPanoramaSymbol symPanelType = pPanel->ClientPtr()->GetPanelType();
	char const *pPanelTypeStr = symPanelType.String();

	int iMap = m_mapV8PanelClassTemplates.Find( symPanelType );
	if( iMap != m_mapV8PanelClassTemplates.InvalidIndex() )
		return m_mapV8PanelClassTemplates.Element( iMap );

	Assert( m_v8ClassTemplateSetupCur.IsEmpty() );
	m_v8ClassTemplateSetupCur = v8::FunctionTemplate::New( m_pV8Isolate );
	m_v8ClassTemplateSetupCur->InstanceTemplate()->SetInternalFieldCount( 1 );
	m_v8ClassTemplateSetupCur->SetClassName( v8::String::NewFromUtf8( m_pV8Isolate, pPanelTypeStr ).ToLocalChecked() );

	Assert( m_v8ClassSignatureSetupCur.IsEmpty() );
	m_v8ClassSignatureSetupCur = v8::Signature::New( m_pV8Isolate, m_v8ClassTemplateSetupCur );

	Assert( m_v8ClassAccessorSignatureSetupCur.IsEmpty() );
	m_v8ClassAccessorSignatureSetupCur = v8::AccessorSignature::New( m_pV8Isolate, m_v8ClassTemplateSetupCur );

	StartRegisterJSScope( pPanelTypeStr );
	
	// Call virtual for panel type to setup what data is exposed
	pPanel->SetupJavascriptObjectTemplate();

	EndRegisterJSScope();

	v8::Persistent<v8::FunctionTemplate> *pTemplate = new v8::Persistent<v8::FunctionTemplate>();
	pTemplate->Reset( m_pV8Isolate, m_v8ClassTemplateSetupCur );

	m_v8ClassTemplateSetupCur.Clear();
	m_v8ClassSignatureSetupCur.Clear();
	m_v8ClassAccessorSignatureSetupCur.Clear();
	
	m_mapV8PanelClassTemplates.Insert( symPanelType, pTemplate );
	
	return pTemplate;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the panorama global v8 context
//-----------------------------------------------------------------------------
void CUIEngine::InitializePanoramaContext( v8::Persistent<v8::Context> *pPersistentContext )
{
	VPROF_BUDGET( "CUIEngine::InitializePanoramaContext", VPROF_BUDGETGROUP_TENFOOT );

	// Create a template for the global object.
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	if( m_V8GlobalTemplate.IsEmpty() )
	{
		v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New( m_pV8Isolate );
		v8::Handle<v8::ObjectTemplate> p = v8::ObjectTemplate::New( m_pV8Isolate );
		int nScope = -1;

		if ( m_vecRegisterJSScopes.Count() == 0 )
		{
			nScope = StartRegisterJSScope( "$" );
		}

		int nEntry;
		RegisterJSType_t rawArgs[1] = { k_ERegisterJSTypeRawV8Args };
		
		// Bind some global functions for use from JS
#define REGFN_NAMED( _Fn, _Name, _Desc ) \
		p->Set( v8::String::NewFromUtf8( m_pV8Isolate, _Name ).ToLocalChecked(), v8::FunctionTemplate::New( m_pV8Isolate, JS##_Fn ) ); \
		nEntry = NewRegisterJSEntry( _Name, RegisterJSEntryInfo_t::k_EGlobalFunction, _Desc, k_ERegisterJSTypeVoid ); \
		SetRegisterJSEntryParams( nEntry, 1, rawArgs, NULL )
#define REGFN( _Fn, _Desc ) REGFN_NAMED( _Fn, #_Fn, _Desc )
		
		REGFN( Msg, "Log a message" );
		REGFN( DefineEvent, "Define an event" );
		REGFN( DefinePanelEvent, "Define an panel event" );
		REGFN( DispatchEvent, "Dispatch an event" );
		REGFN( DispatchEventAsync, "Dispatch an event to occur later" );
		REGFN( RegisterEventHandler, "Register an event handler" );
		REGFN( UnregisterEventHandler, "Remove an event handler" );
		REGFN( RegisterForUnhandledEvent, "Register a handler for an event that is not otherwise handled" );
		REGFN( UnregisterForUnhandledEvent, "Remove an unhandled event handler" );
		REGFN( FindChildInContext, "Find an element" );
		REGFN( AsyncWebRequest, "Make a web request" );
		REGFN_NAMED( PanelConstructor, "CreatePanel", "Create a new panel" );
		REGFN( Localize, "Localize a string" );
		REGFN( Language, "Get the current language" );
		REGFN( Schedule, "Schedule a function to be called later" );
		REGFN( CancelScheduled, "Cancelse a scheduled function" );
		REGFN( GetContextPanel, "Get the current panel context" );
		REGFN( RegisterKeyBind, "Register a key binding" );
		REGFN( Each, "Call a function on each given item" );
		REGFN( DbgIsReloadingScript, "Call during JS startup code to check if script is being reloaded" );
		REGFN( UrlEncode, "$.UrlEncode(str).  Encodes str, which must be 2048 utf-8 bytes or shorter, into URL-encoded form." );
		REGFN( UrlDecode, "$.UrlDecode(str).  Decodes str, which must be 2048 utf-8 bytes or shorter, from URL-encoded form." );
		REGFN( HTMLEscape, "$.HTMLEscape(str, truncate=false).  Converts str, which must be 2048 utf-8 bytes or shorter, into an HTML-safe version.  If truncate=true, too long strings will be truncated instead of throwing an exception" );

#undef REGFN

		if ( nScope >= 0 )
		{
			EndRegisterJSScope();
		}

		FOR_EACH_VEC( m_vecV8GlobalFunctionRegistrations, i )
		{
			const V8GlobalFunctionRegistration_t &reg = m_vecV8GlobalFunctionRegistrations[i];
			v8::Local<v8::FunctionTemplate> func = v8::Local<v8::FunctionTemplate>::New( m_pV8Isolate, *(reg.m_pFunction) );

			if( !reg.m_bTrueGlobal )
				p->Set( v8::String::NewFromUtf8( m_pV8Isolate, reg.m_strName.String() ).ToLocalChecked(), func );
			else
				global->Set( v8::String::NewFromUtf8( m_pV8Isolate, reg.m_strName.String() ).ToLocalChecked(), func );
		}

		// Register selector call as function shorthand
		p->SetCallAsFunctionHandler( JSDollarSign );

		m_V8PanoramaTemplate.Reset( m_pV8Isolate, p );

		// Setup p object in global namespace now
		global->Set( v8::String::NewFromUtf8( m_pV8Isolate, "panorama" ).ToLocalChecked(), p );

		m_V8GlobalTemplate.Reset( m_pV8Isolate, global );
	}

	v8::Handle<v8::ObjectTemplate> global = v8::Local<v8::ObjectTemplate>::New( m_pV8Isolate, m_V8GlobalTemplate );
	v8::Handle<v8::Context> handle_context = v8::Context::New( m_pV8Isolate, NULL, global );
	
	v8::Context::Scope context_scope( handle_context );

	v8::Handle<v8::Object> objGlobal = handle_context->Global();
	v8::Local< v8::Value > objPanoramaVal = objGlobal->Get( handle_context, v8::String::NewFromUtf8( m_pV8Isolate, "panorama" ).ToLocalChecked() ).ToLocalChecked();
	v8::Local< v8::Object > objPanorama = v8::Local< v8::Object >::Cast( objPanoramaVal );

	objGlobal->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, "$" ).ToLocalChecked(), objPanorama );

	FOR_EACH_VEC( m_vecV8GlobalObjectRegistrations, i )
	{
		const V8GlobalObjectRegistration_t &reg = m_vecV8GlobalObjectRegistrations[i];
		v8::Local<v8::Object> obj = v8::Local<v8::Object>::New( m_pV8Isolate, *(reg.m_pObj) );

		if( !reg.m_bTrueGlobal )
			objPanorama->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, reg.m_strName.String() ).ToLocalChecked(), obj );
		else
			objGlobal->Set( v8::Isolate::GetCurrent()->GetCurrentContext(), v8::String::NewFromUtf8( m_pV8Isolate, reg.m_strName.String() ).ToLocalChecked(), obj );
	}

#if defined( OSX ) && defined( __aarch64__ ) && defined( NO_STEAM )
	// Global native objects are installed in every Panorama context. The missing
	// offline component contracts must likewise be visible in each panel context.
	if ( const char *overlay = CommandLine()->ParmValue( "-panorama_native_overlay", (const char *)nullptr ) )
	{
		CUtlBuffer bridge;
		CFmtStr path( "%s/panorama/scripts/common/native_offline_ui.js", overlay );
		if ( g_pFullFileSystem->ReadFile( path, nullptr, bridge ) )
		{
			v8::Local<v8::String> source = v8::String::NewFromUtf8( m_pV8Isolate, (const char *)bridge.Base(), v8::NewStringType::kNormal, bridge.TellPut() ).ToLocalChecked();
			v8::Local<v8::Script> script;
			v8::TryCatch error( m_pV8Isolate );
			if ( v8::Script::Compile( handle_context, source ).ToLocal( &script ) )
			{
				v8::Local<v8::Value> ignored;
				if ( !script->Run( handle_context ).ToLocal( &ignored ) ) OutputJSExceptionToConsole( error, nullptr );
			}
			else OutputJSExceptionToConsole( error, nullptr );
		}
	}
#endif
	pPersistentContext->Reset( m_pV8Isolate, handle_context );


#if V8_DEBUGGING_ENABLED
	if ( m_pInspectorClient )
	{
		m_pInspectorClient->ContextCreated( handle_context );		
	}
#endif

	V8_CtxDbgMsg( "Estimated context memory now %d KB\n", handle_context->EstimatedSize() / 1024 );

}


//-----------------------------------------------------------------------------
// Purpose: Helper to output a string to the panorama debugger and the Windows debugger
//-----------------------------------------------------------------------------
void CUIEngine::OutputJSString( const IUIPanel *pContext, const char *pchString, bool bException )
{
	::DispatchEvent( JSConsoleOutput(), (IUIPanel*)NULL, pContext, pchString );

#if defined( SOURCE2_PANORAMA )
	if ( bException )
	{
		Log_Warning( LOG_PANORAMA_SCRIPT, Color(255,255,0), "%s\n", pchString );
	}
	else
	{
		Log_Msg( LOG_PANORAMA_SCRIPT, "%s\n", pchString );
	}
#else
	DevMsg( "%s\n", pchString );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Helper to output exception to JS debug console
//-----------------------------------------------------------------------------
void CUIEngine::OutputJSExceptionToConsole( v8::TryCatch &try_catch, IUIPanel *pPanelContext )
{
	static const bool s_bShowDialogByDefault =
#if V8_DEBUGGING_ENABLED
		CommandLine()->CheckParm("-nojserrors") ? false							// Running with -nojserrors means forced no dialog (from P4)
		: ( CommandLine()->CheckParm( "-dojserrors" ) ? true					// Running with -dojserrors means forcing dialog (from Steam depot)
			: !g_pFullFileSystem->FileExists( PANORAMA_ZIPFILE_NAME, NULL ) )	// code.pbin packaged file is in Steam depot only, show dialog when from P4 without code.pbin
#else
		false
#endif
		;
#if V8_DEBUGGING_ENABLED
	static bool s_bShowDialogIngoreAllExceptions = false;	// Allow user to suppress all future JS error dialogs
#endif

	// Check the exception and bail out early
	v8::String::Utf8Value error(v8::Isolate::GetCurrent(),  try_catch.Exception() );
	CUtlString strOut;

	v8::Local< v8::Message > message = try_catch.Message();

	if( !message.IsEmpty() )
	{
		v8::Local< v8::Context > context = m_pV8Isolate->GetCurrentContext();
		v8::String::Utf8Value script_name(v8::Isolate::GetCurrent(),  message->GetScriptOrigin().ResourceName() );
		int nLineNumber = message->GetLineNumber( context ).FromMaybe( 0 );
		int nColumnNumber = message->GetStartColumn();

		strOut.Format( "!! (%s, line:%d, col:%d) - %s", V8ToCString( script_name ), nLineNumber, nColumnNumber, V8ToCString ( error ) );

#if V8_DEBUGGING_ENABLED || V8_EXCEPTIONS_BREAKPAD_CAPTURE_ENABLED

		strOut.Format( "JS Exception! *** Skipping rest of script *** \n\n%s\nline:%d, col:%d\n\n", V8ToCString( script_name ), nLineNumber, nColumnNumber );

		v8::Local< v8::String > sourceline;
		if ( message->GetSourceLine( context ).ToLocal( &sourceline ) ) 
		{
			// Print line of source code.
			v8::String::Utf8Value sourcelinevalue(v8::Isolate::GetCurrent(), sourceline);
			const char* sourceline_string = V8ToCString(sourcelinevalue);

			CUtlString temp = strOut;
			strOut.Format( "%s>> %s\n\n", strOut.String(), sourceline_string );
		}

		v8::Local< v8::Value > stack_trace_string;
		if ( try_catch.StackTrace( context ).ToLocal( &stack_trace_string ) && stack_trace_string->IsString() ) 
		{
			v8::String::Utf8Value stack_trace(v8::Isolate::GetCurrent(),  v8::Local< v8::String >::Cast( stack_trace_string ) );
			CUtlString temp = strOut;
			strOut.Format( "%s%s\n", strOut.String(), *stack_trace );
		}
		else
		{
			CUtlString temp = strOut;
			strOut.Format( "%s%s\n", strOut.String(), V8ToCString( error ) );
		}

#if V8_EXCEPTIONS_BREAKPAD_CAPTURE_ENABLED

#if V8_DEBUGGING_ENABLED
		if ( !s_bShowDialogByDefault )	// If we run with forced dialog, then don't upload to breakpad
#endif
		{
			//
			// <vitaliy, 2018>
			// Submit this exception to breakpad, syntax must be \nJS(...)\n...
			// see: sys_dll2.cpp CErrorText::BuildComment
			// and custom breakpad matching in externalProcessor.py
			// (https://intranet.valvesoftware.com/wiki/Minidumps)
			//

			CUtlString fmtForDump;
			const char *szScriptName = V8ToCString( script_name );
			if ( const char *szSlash1 = strrchr( szScriptName, '/' ) )
				szScriptName = szSlash1 + 1;
			if ( const char *szSlash2 = strrchr( szScriptName, '\\' ) )
				szScriptName = szSlash2 + 1;
			fmtForDump.Format( "\nJS(%s:%d:%d)\n%s\n%s\n", szScriptName, nLineNumber, nColumnNumber, V8ToCString( error ), strOut.Access() );
			g_pEngineclient->ExecuteExceptionHandler( fmtForDump.Access() );
		}

#endif
#endif
	}
	else
	{
		strOut.Format( "JS Exception!!\n\n(undefined:0) - %s", V8ToCString( error ) );
	}
	
	if ( CommandLine()->CheckParm( "-m5diagnostics" ) )
		fprintf( stderr, "%s\n", strOut.String() );
	if ( pPanelContext )
	{
		OutputJSString( pPanelContext, strOut.String(), true );
	}

#if V8_DEBUGGING_ENABLED
	if ( s_bShowDialogByDefault && !s_bShowDialogIngoreAllExceptions )
	{	
		
// #if V8_DEBUGGING_ENABLED
		void DebuggerRunFrame( void );
		switch ( V8_AssertDlg( strOut.String(), DebuggerRunFrame ) )
// #else
// 		switch ( V8_AssertDlg( strOut.String(), nullptr ) )
// #endif
		{
		case V8_ASSERT_DLG_IGNORE_ALL:
			s_bShowDialogIngoreAllExceptions = true; // suppress all future JS error dialogs
			break;

		case V8_ASSERT_DLG_DEBUG:
			DebuggerBreakIfDebugging();
			break;
		}
	}
#endif // V8_DEBUGGING_ENABLED && bShowDialog
}


//-----------------------------------------------------------------------------
// Purpose:  Get panel that contains the javascript context. 
//
//	NOTE!
//
//	Panel ptr is encoded in the context's security token. Therefore,
//	there is no one-one mapping between panel and context in the case 
//  where multiple panels are running scripts in the global context.
//
//	This function is only safe to call from within JS, because the correct 
//	panel is either on top of the panel stack, in case the script is running
//	int the global context, or can be retrieved from the context itself, in case
//	the script is running inside a local context.
//
//-----------------------------------------------------------------------------
panorama::IUIPanel *CUIEngine::GetPanelForJavaScriptContext( v8::Context *pContext )
{
	if ( gPanelStackTop <= 1 )
	{
		V8_CtxDbgAssert( "GetPanelForJavaScriptContext called from outside JS!" );
	}
		
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

 	uint64 secToken = GetContextSecurityToken( pContext );
	if ( secToken == GLOBAL_CONTEXT_SEC_TOKEN )
	{
		// Global context: the correct panel should be on top of panels stack
		return gPanelStack[ gPanelStackTop ];
	}

	CPanelPtr<IUIPanel> panelPtr;
	panelPtr.SetFromUInt64( secToken );

	IUIPanel *pPanel = panelPtr.Get();

	if ( pPanel != gPanelStack[ gPanelStackTop ] )
	{
		V8_CtxDbgMsg( "Context panel mismatch. Panel = %x (%s), context %llx, but stack top = %x (%s), parent = %x (%s)\n",
			pPanel, pPanel->GetID(),
			GetContextSecurityToken( pContext ),
			gPanelStack[ gPanelStackTop ],
			gPanelStack[ gPanelStackTop ]->GetID(),
			gPanelStack[ gPanelStackTop ]->GetJavaScriptContextParent(),
			gPanelStack[ gPanelStackTop ]->GetJavaScriptContextParent()->GetID() );

		V8_CtxDbgAssert( "Context panel mismatch" );
	}

	V8_CtxDbgMsg( "GetPanelForJavaScriptContext returned panel %x (%s)\n", pPanel, pPanel->GetID() );
	
	return pPanel;
}

//-----------------------------------------------------------------------------
// Purpose:  Get panel that contains the javascript context
//-----------------------------------------------------------------------------
v8::Persistent<v8::Context> *CUIEngine::GetJavaScriptContextForPanel( panorama::IUIPanel *pPanel )
{
	return GetContextForPanel( pPanel );
}

//-----------------------------------------------------------------------------
// Purpose:  Compile some script, but keep it for later to run rather than running now
//-----------------------------------------------------------------------------
v8::Persistent<v8::Script> *CUIEngine::CompileScript( IUIPanel *pPanelContext, const char *pchScriptString, const char *pchSourceFileName )
{
	VPROF_BUDGET( "CUIEngine::CompileScript", VPROF_BUDGETGROUP_TENFOOT );
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	v8::Persistent<v8::Script> *pScriptOut = new v8::Persistent<v8::Script>;

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pPanelContext );
	
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *pContextRef );
	v8::Context::Scope context_scope( context );

	V8_CtxDbgMsg( "Compiling script: Panel = %x, Context = %llx, %s\n", pPanelContext, GetContextSecurityToken( *context ), pchScriptString );

	v8::Handle<v8::String> source = v8::String::NewFromUtf8( m_pV8Isolate, pchScriptString ).ToLocalChecked();
	

	v8::TryCatch try_catch( v8::Isolate::GetCurrent() );

	v8::ScriptOrigin origin = v8::ScriptOrigin(
		v8::String::NewFromUtf8( m_pV8Isolate, pchSourceFileName ).ToLocalChecked(), v8::Integer::NewFromUnsigned( m_pV8Isolate, 0 ), v8::Integer::NewFromUnsigned( m_pV8Isolate, 0 ) );

	v8::Handle<v8::Script> script = v8::Script::Compile( v8::Isolate::GetCurrent()->GetCurrentContext(), source, &origin ).FromMaybe( v8::Local<v8::Script>() );
	if( script.IsEmpty() )
	{
		OutputJSExceptionToConsole( try_catch, pPanelContext );
		return pScriptOut;
	}

	pScriptOut->Reset( m_pV8Isolate, script );
	return pScriptOut;
}


//-----------------------------------------------------------------------------
// Purpose: Get/create javascript context for panel
//-----------------------------------------------------------------------------
v8::Persistent<v8::Context> *CUIEngine::GetContextForPanel( const IUIPanel *pPanel )
{
	IUIPanel *pPanelContext = pPanel->GetJavaScriptContextParent();
	v8::Persistent<v8::Context> *pContext = GetContextForPanelInternal( pPanelContext );

	return pContext;
}

//-----------------------------------------------------------------------------
// Purpose: Get/create javascript context for panel
//-----------------------------------------------------------------------------
v8::Persistent<v8::Context> *CUIEngine::GetContextForPanelInternal( IUIPanel *pPanel )
{
	bool bUsesGlobalContext = true;
	v8::Persistent<v8::Context> *pContextRef = &(m_V8UIEngineGlobalContext);
		
	if ( pPanel && !pPanel->BUsesGlobalContext() )
	{
		int iMap = m_MapPanelV8Contexts.Find( pPanel );
		if( iMap != m_MapPanelV8Contexts.InvalidIndex() )
		{
			pContextRef = m_MapPanelV8Contexts[iMap];
		}
		else
		{
			v8::Persistent<v8::Context> *pNewContext = new v8::Persistent<v8::Context>();

			iMap = m_MapPanelV8Contexts.Insert( pPanel, pNewContext );
			pContextRef = m_MapPanelV8Contexts[iMap];

			InitializePanoramaContext( m_MapPanelV8Contexts[iMap] );
			
			// Note template parameter irrelevant in following use of CPanelPtr
			CPanelPtr< IUIPanel > ptrPanel = pPanel;
			SetContextSecurityToken( m_pV8Isolate, *(pContextRef->Get(m_pV8Isolate)), ptrPanel.GetHandleAsUInt64() );		
		}
		
		bUsesGlobalContext = false;
	}

	if ( !bUsesGlobalContext )
	{	
		v8::Context *pContextPtr = *( pContextRef->Get( m_pV8Isolate ) );
		uint64 contextToken = GetContextSecurityToken( pContextPtr );
		V8_CtxDbgMsg( "GetContextForPanel: Context panel %x (%s), context tok = %llx (%s)\n", pPanel, pPanel->GetID(), contextToken,
			bUsesGlobalContext? "global ctx": "non-global ctx" );
	}

	return pContextRef;
}


//-----------------------------------------------------------------------------
// Purpose: Run pre-complied javascript within specified (or UIEngine global context if null) panel context
//-----------------------------------------------------------------------------
v8::Handle< v8::Value > CUIEngine::RunFunction( IUIPanel *pPanelContext, v8::Persistent<v8::Function> *pFunction, 
	int nNumArgs, v8::Handle<v8::Value> *pArgs, bool bPrintRetValue )
{
	VPROF_BUDGET( "CUIEngine::RunFunction", VPROF_BUDGETGROUP_TENFOOT );

	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pPanelContext );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *pContextRef );

	v8::Context::Scope context_scope( context );

	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( m_pV8Isolate, *(pFunction) );
	v8::Handle<v8::Object> obj = context->Global();

	v8::Handle<v8::Value> returnval = RunJSFunctionInternal( pPanelContext, context, obj, fnLocal, nNumArgs, pArgs, bPrintRetValue );
	return returnval;
}

//-----------------------------------------------------------------------------
// Purpose: Run pre-complied javascript within specified (or UIEngine global context if null) panel context
//-----------------------------------------------------------------------------
v8::Handle< v8::Value > CUIEngine::RunFunction( IUIPanel *pPanel, const char *pchFunctionName, int nNumArgs, v8::Handle<v8::Value> *pArgs )
{
	VPROF_BUDGET( "CUIEngine::RunFunction", VPROF_BUDGETGROUP_TENFOOT );

	v8::Isolate *pIsolate = UIEngine()->GetV8Isolate();
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pPanel );	
	v8::Handle< v8::Context > context = v8::Local< v8::Context >::New( pIsolate, *pContextRef );
	v8::Context::Scope context_scope( context );

	v8::Handle< v8::Object > global = context->Global();
	v8::Handle< v8::Value > functionName = global->Get( v8::Isolate::GetCurrent()->GetCurrentContext(),  v8::String::NewFromUtf8( pIsolate, pchFunctionName ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
	v8::Handle< v8::Function > fnLocal = v8::Handle< v8::Function >::Cast( functionName );

	v8::Handle<v8::Value> returnval = RunJSFunctionInternal( pPanel, context, global, fnLocal, nNumArgs, pArgs, false );
	return returnval;
}

//-----------------------------------------------------------------------------
// Purpose: Run pre-complied javascript within specified (or UIEngine global context if null) panel context
//
//	Scripts are usually bound to a context when created however we don't see a way to get that context from the script.
//	That is why you need to provide it as pPanelContext (for logging and we will run the script unbound for consistency)
//-----------------------------------------------------------------------------
void CUIEngine::RunScript( IUIPanel *pPanelContext, v8::Persistent<v8::Script> *pScript, bool bPrintRetValue )
{
	VPROF_BUDGET( "CUIEngine::RunScript (pre-compiled)", VPROF_BUDGETGROUP_TENFOOT );

	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pPanelContext );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *pContextRef );

	v8::Context::Scope context_scope( context );

	v8::Local<v8::Script> localScript = v8::Local<v8::Script>::New( m_pV8Isolate, *pScript );
	v8::Local<v8::UnboundScript> unboundScript = localScript->GetUnboundScript();
	localScript = unboundScript->BindToCurrentContext();

#if ( V8_CTX_DBG_SPEW_ENABLED )
	v8::String::Utf8Value scriptName(v8::Isolate::GetCurrent(),  pScript->Get( m_pV8Isolate )->GetUnboundScript()->GetScriptName() );
	V8_CtxDbgMsg( "RunScript <no file>: Panel %x, context %llx, %s\n", pPanelContext, 
		GetContextSecurityToken( *context ), *scriptName);
#endif

	v8::Handle<v8::Value> returnval = RunJSScriptInternal( pPanelContext, localScript, bPrintRetValue, false );

	return;
}


//-----------------------------------------------------------------------------
// Purpose: Run javascript within specified (or UIEngine global context if null) panel context
//-----------------------------------------------------------------------------
void CUIEngine::RunScript( IUIPanel *pPanelContext, const char *pchScriptString, const char *pchScriptFile, 
	int nSourceBeginLine, int nSourceBeginCol, bool bPrintRetValue, bool bIsReload )
{
	VPROF_BUDGET( "CUIEngine::RunScript (compile+run)", VPROF_BUDGETGROUP_TENFOOT );
	
	char szAbsolutePathScratch[MAX_PATH + 1];
	if ( !V_strnicmp( "file://", pchScriptFile, 7 ) || !V_strnicmp( "s2r://", pchScriptFile, 6 ) || !V_strnicmp( "raw://", pchScriptFile, 6 ) )
	{
		CFileResource file( pchScriptFile );
		const CUtlString &fileName = file.GetReferencePath();
		if ( !fileName.IsEmpty() && file.BIsLocalPath() )
		{
			g_pFullFileSystem->RelativePathToFullPath( fileName, "MOD", szAbsolutePathScratch, sizeof( szAbsolutePathScratch ) );
		}
		else
		{
			szAbsolutePathScratch[0] = '\0';
		}
	}
	else
	{
		g_pFullFileSystem->RelativePathToFullPath( pchScriptFile, "MOD", szAbsolutePathScratch, sizeof( szAbsolutePathScratch ) );
	}

	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pPanelContext );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *pContextRef );

	v8::Context::Scope context_scope( context );

	V8_CtxDbgMsg( "RunScript: Panel = %x, context = %llx, script = %s\n", pPanelContext, 
		GetContextSecurityToken( *context ), szAbsolutePathScratch );

	v8::Handle<v8::String> source = v8::String::NewFromUtf8( m_pV8Isolate, pchScriptString ).ToLocalChecked();

	v8::Handle<v8::Script> script;

	{
		v8::TryCatch try_catch( v8::Isolate::GetCurrent() );

		v8::ScriptOrigin origin = v8::ScriptOrigin(
			v8::String::NewFromUtf8( m_pV8Isolate, szAbsolutePathScratch ).ToLocalChecked(), 
			v8::Integer::NewFromUnsigned( m_pV8Isolate, nSourceBeginLine ), 
			v8::Integer::NewFromUnsigned( m_pV8Isolate, nSourceBeginCol ) );
	
		script = v8::Script::Compile( v8::Isolate::GetCurrent()->GetCurrentContext(), source, &origin ).FromMaybe( v8::Local<v8::Script>() );
		if ( script.IsEmpty() )
		{
			OutputJSExceptionToConsole( try_catch, pPanelContext );
			return;
		}
	}

#if V8_DEBUGGING_ENABLED
	// Give the debug front end a chance to set breakpoints in the script we just loaded
	if ( panorama_remote_debug.GetBool() && CommandLine()->CheckParm( "-panorama_wait_debugger") )
	{
		if ( IsWebSocketServerConnected() )
		{
			m_pWebsocketServer->RunFrame( panorama_dbg_brkpts_timeout_ms.GetInt() );
		}
	}
#endif	// V8_DEBUGGING_ENABLED

	v8::Handle<v8::Value> returnval = RunJSScriptInternal( pPanelContext, script, false, bIsReload );
	
	return;
}

//-----------------------------------------------------------------------------
// RunJSFunctionInternal
//-----------------------------------------------------------------------------

v8::Local<v8::Value> CUIEngine::RunJSFunctionInternal( IUIPanel *pPanelContext, v8::Local< v8::Context> context, v8::Local<v8::Value> recv, 
	v8::Local<v8::Function> jsfn, int argc, v8::Local<v8::Value> argv[], bool bPrintRetValue )
{
	v8::Local<v8::Value> retVal;

#if ( V8_CTX_DBG_SPEW_ENABLED )
	v8::Local< v8::Value> fnNameVal = jsfn->GetName();
	v8::String::Utf8Value fnNameStr(v8::Isolate::GetCurrent(), fnNameVal);
	V8_CtxDbgMsg("RunFunction: Panel %x, context %llx, %s\n", pPanelContext,
		GetContextSecurityToken(*context), *fnNameStr);
#endif

#if V8_DEBUGGING_ENABLED

	if ( IsWebSocketServerConnected() )
	{
		PushContextPanel( pPanelContext );
		retVal = jsfn->Call( v8::Isolate::GetCurrent()->GetCurrentContext(),  recv, argc, argv ).FromMaybe( v8::Local<v8::Value>() );
		PopContextPanel();
	}
	else
	{

#endif

	#if V8_ENABLE_EXCEPTIONS

		v8::TryCatch try_catch( v8::Isolate::GetCurrent() );
	#endif

		PushContextPanel( pPanelContext );
		retVal = jsfn->Call( v8::Isolate::GetCurrent()->GetCurrentContext(),  recv, argc, argv ).FromMaybe( v8::Local<v8::Value>() );
		PopContextPanel();
	
	#if V8_ENABLE_EXCEPTIONS

		// Output any exception we caught
		if( try_catch.HasCaught() )
		{
			UIEngineInternal()-> OutputJSExceptionToConsole( try_catch, pPanelContext );
		}

	#endif

#if V8_DEBUGGING_ENABLED
	
	}

#endif

	if( bPrintRetValue )
	{
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  retVal );

		CUtlString strOut;
		strOut.Format( "=> %s", V8ToCString( str ) );
		OutputJSString( pPanelContext, strOut.String());
	}

	return retVal;
}

//-----------------------------------------------------------------------------
// RunJSScriptInternal
//-----------------------------------------------------------------------------

v8::Local<v8::Value> CUIEngine::RunJSScriptInternal( IUIPanel *pPanelContext, v8::Local<v8::Script> script, bool bPrintRetValue, bool bIsReload )
{
	v8::Local<v8::Value> retVal;

#if V8_DEBUGGING_ENABLED

	if ( IsWebSocketServerConnected() )
	{
		PushContextPanel( pPanelContext );
		retVal = script->Run( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Value>() );
		PopContextPanel();
	}
	else
	{

#endif

	#if V8_ENABLE_EXCEPTIONS

		v8::TryCatch try_catch( v8::Isolate::GetCurrent() );

	#endif

		PushContextPanel( pPanelContext );
		m_bIsReloadingScript = bIsReload;

		retVal = script->Run( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Value>() );
		
		m_bIsReloadingScript = false;
		PopContextPanel();
	
	#if V8_ENABLE_EXCEPTIONS

		// Output any exception we caught
		if( try_catch.HasCaught() )
		{
			UIEngineInternal()->OutputJSExceptionToConsole( try_catch, pPanelContext );
		}

	#endif

#if V8_DEBUGGING_ENABLED
	
	}

#endif

	if( bPrintRetValue )
	{
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  retVal );

		CUtlString strOut;
		strOut.Format( "=> %s", V8ToCString( str ) );
		OutputJSString( pPanelContext, strOut.String());
	}

	return retVal;
}

//-----------------------------------------------------------------------------
// Purpose: Callback from window when it is being destroyed
//-----------------------------------------------------------------------------
bool CUIEngine::OnWindowShutdown( IUIWindow *pIWindow )
{	
	CTopLevelWindow *pWindow = (CTopLevelWindow*)pIWindow;

	// Just need to update our list, window will destroy itself and notify children already
	FOR_EACH_VEC_BACK( m_vecWindows, i )
	{
		if ( m_vecWindows[i] == pWindow )
		{
			m_vecWindows.Remove(i);
			break;
		}
	}	

	if ( m_MapV8IUIWindowObjectInstances.Find( pIWindow ) )
	{
		m_MapV8IUIWindowObjectInstances.Remove( pIWindow );
	}

	if ( pWindow == m_pPanelZooWindow )
		m_pPanelZooWindow = NULL;

	if ( pWindow == m_pConsoleWindow )
		m_pConsoleWindow = NULL;

	m_pInputEngine->OnWindowShutdown( pWindow );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Is the engine still running?
//-----------------------------------------------------------------------------
bool CUIEngine::BIsRunning()
{
	return !m_bShutdown && !m_bShuttingdown;
}


//-----------------------------------------------------------------------------
// Purpose: Check if any window we own has focus
//-----------------------------------------------------------------------------
bool CUIEngine::BAnyWindowHasFocus()
{
	FOR_EACH_VEC( m_vecWindows, i )
	{
		if ( m_vecWindows[i]->BHasFocus() )
			return true;
	}

	return false;
}



//-----------------------------------------------------------------------------
// Purpose: Check if any window we own has focus
//-----------------------------------------------------------------------------
bool CUIEngine::BAnyVisibleWindowHasFocus()
{
	FOR_EACH_VEC( m_vecWindows, i )
	{
		if( m_vecWindows[i]->BIsVisible() && m_vecWindows[i]->BHasFocus() )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Check if any overlay window we own has focus
//-----------------------------------------------------------------------------
bool CUIEngine::BAnyOverlayWindowHasFocus()
{
	FOR_EACH_VEC( m_vecWindows, i )
	{
		if ( m_vecWindows[i]->BIsOverlay() && m_vecWindows[i]->BHasFocus() )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Get the focused window, there should really be only one, if some bug 
// allows multiple the first found is returned
//-----------------------------------------------------------------------------
IUIWindow *CUIEngine::GetFocusedWindow( bool bSkipVRWindows )
{	
	FOR_EACH_VEC( m_vecWindows, i )
	{
		if ( m_vecWindows[i]->BHasFocus() && ( !bSkipVRWindows || !m_vecWindows[i]->BIsVROverlay() ) )
			return m_vecWindows[i];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Perform shutdown
//-----------------------------------------------------------------------------
void CUIEngine::Shutdown()
{
	m_bShuttingdown = true;

	// drop all cookie containers
	FOR_EACH_MAP( m_mapDomainCookieContainers, i )
	{
		ClientHTTP()->ReleaseCookieContainer( m_mapDomainCookieContainers.Element( i ) );
	}
	m_mapDomainCookieContainers.RemoveAll();

	// Copy here because a common response to shutdown may be to delete the listener, which could
	// end up modifying the original vector
	CUtlVector< IUIEngineFrameListener * > vecListeners;
	vecListeners.AddMultipleToTail( m_vecFrameListeners.Count(), m_vecFrameListeners.Base() );
	FOR_EACH_VEC( vecListeners, i )
	{
		vecListeners[i]->OnEngineShutdown();
	}

	m_bDebuggerActive = false;
	if( m_pPanelZooWindow )
	{
		m_pPanelZooWindow->Delete();
		m_pPanelZooWindow = NULL;
	}

	if( m_pConsoleWindow )
	{
		m_pConsoleWindow->Delete();
		m_pConsoleWindow = NULL;
	}

	while( m_vecWindows.Count() )
	{
		// Deleting will end up calling back into us to remove from vector.
		delete m_vecWindows[m_vecWindows.Count()-1];
	}

	m_bShutdown = true;
	if ( m_bInited )
	{
		m_bInited = false;

#if !defined( PANORAMA_DISABLE_VIDEO )
		VideoPlaybackShutdown();
#endif

		::UnregisterForUnhandledEvent( ReloadChangedUIFiles(), this, &CUIEngine::AutoReloadChangedFiles );

#if DEVELOPMENT_ONLY
		::UnregisterForUnhandledEvent( ReloadPanorama(), this, &CUIEngine::OnReloadPanorama );
		::UnregisterForUnhandledEvent( ForceReloadPanorama(), this, &CUIEngine::OnForceReloadPanorama );
		::UnregisterForUnhandledEvent( ToggleDebugger(), this, &CUIEngine::OnToggleDebug );
		::UnregisterForUnhandledEvent( ShowPanelZoo(), this, &CUIEngine::OnShowPanelZoo );
		::UnregisterForUnhandledEvent( DumpMemory(), this, &CUIEngine::OnMemDump );
#endif


		::UnregisterForUnhandledEvent( ToggleConsole(), this, &CUIEngine::OnToggleConsole );
		::UnregisterForUnhandledEvent( TopLevelWindowClose(), this, &CUIEngine::OnWindowShutdown );
		::UnregisterForUnhandledEvent( DeletePanel(), this, &CUIEngine::OnDeletePanel );
		::UnregisterForUnhandledEvent( SetInputFocus(), this, &CUIEngine::OnSetInputFocus );
		::UnregisterForUnhandledEvent( DropInputFocus(), this, &CUIEngine::OnDropInputFocus );
		::UnregisterForUnhandledEvent( CopyStringToClipboard(), this, &CUIEngine::OnCopyStringToClipboard );
		::UnregisterForUnhandledEvent( ReloadStyleFile(), this, &CUIEngine::OnReloadStyleFile );
		::UnregisterForUnhandledEvent( SetAllChildrenActivationEnabled(), this, &CUIEngine::OnSetAllChildrenActivationEnabled );
		::UnregisterForUnhandledEvent( AsyncEvent(), this, &CUIEngine::OnAsyncEvent );
		::UnregisterForUnhandledEvent( JSScheduledFunction(), this, &CUIEngine::OnJSScheduledFunction );
		::UnregisterForUnhandledEvent( SetPanelSelected(), this, &CUIEngine::OnSetPanelSelected );
		::UnregisterForUnhandledEvent( TogglePanelSelected(), this, &CUIEngine::OnTogglePanelSelected );
		::UnregisterForUnhandledEvent( SetChildPanelsSelected(), this, &CUIEngine::OnSetChildPanelsSelected );

		m_pLocalization->Shutdown();

		SAFE_DELETE( m_pLocalization );
		SAFE_DELETE( m_pSoundSystem );

		SAFE_DELETE( m_pInputEngine );
		SAFE_DELETE( m_pUILayoutManager );

		UITextServices()->ShutdownServices();
#if !defined( SOURCE2_PANORAMA ) 
		SAFE_DELETE( g_IUITextServices );
#endif
	}

	// clear after unregistering for unhandled events
	FOR_EACH_HASHMAP( m_mapUnhandledEventHandlers, i )
	{
		delete m_mapUnhandledEventHandlers.Element( i );
	}
	m_mapUnhandledEventHandlers.RemoveAll();

	FOR_EACH_HASHMAP( m_mapUnhandledEventHandlerMessages, i )
	{
		delete m_mapUnhandledEventHandlerMessages.Element( i );
	}
	m_mapUnhandledEventHandlerMessages.RemoveAll();

	FOR_EACH_MAP_FAST( m_mapPanelToJSUnhandledEventHandlers, i )
	{
		delete m_mapPanelToJSUnhandledEventHandlers.Element( i );
	}
	m_mapPanelToJSUnhandledEventHandlers.RemoveAll();

	m_AllJSGenericCallbacks.PurgeAndDeleteElements();

	FOR_EACH_MAP_FAST( m_mapPanelToJSGenericCallbacks, i )
	{
		delete m_mapPanelToJSGenericCallbacks.Element( i );
	}
	m_mapPanelToJSGenericCallbacks.RemoveAll();

	m_vecDirWatchers.PurgeAndDeleteElements();
	
	FOR_EACH_MAP_FAST( m_mapNamedOverwritePaths, i )
	{
		m_mapNamedOverwritePaths[i]->RemoveAll();
	}
	m_mapNamedOverwritePaths.RemoveAll();

	m_vecXHeaders.Purge();

	// Free queued async events that will now never happen
	QueuedEvent_t queued;
	while( m_tslNewAsyncEvents.Count() )
	{
		if( m_tslNewAsyncEvents.PopItem( &queued ) )
			delete queued.pEvent;
	}
	m_tslNewAsyncEvents.RemoveAll();

	FOR_EACH_VEC( m_vecQueuedEvents, i )
	{
		delete m_vecQueuedEvents[i].pEvent;
	}
	m_vecQueuedEvents.RemoveAll();


#if !defined( SOURCE2_PANORAMA )
	// ugly that this is global... same with init call.
	Common_FreeGlobals();

	vrapi::ClearInterfaces();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: turn on the subsystems we use
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
bool CUIEngine::StartupSubsystems( IUISettings *pSettings, PlatWindow_t hWindow )
#else
bool CUIEngine::StartupSubsystems( IUISettings *pSettings, IHTMLChromeController *pHTMLController )
#endif
{
	m_bInited = true;
	m_pSettings = pSettings;
#if !defined( SOURCE2_PANORAMA )
	m_pHTMLController = pHTMLController;
	if( m_pHTMLController )
		m_pHTMLController->Init();
#endif 

	m_pLocalization = new CLocalization();
	m_pInputEngine->Initialize( pSettings );
	m_pLocalization->SetLanguage( m_pSettings ? m_pSettings->GetUILanguage() : "english" );
	m_pUILayoutManager = new CLayoutManager();
	m_pSoundSystem = CreateSoundSystem();

	::RegisterForUnhandledEvent( ReloadChangedUIFiles(), this, &CUIEngine::AutoReloadChangedFiles );

#if DEVELOPMENT_ONLY
	::RegisterForUnhandledEvent( ReloadPanorama(), this, &CUIEngine::OnReloadPanorama );
	::RegisterForUnhandledEvent( ForceReloadPanorama(), this, &CUIEngine::OnForceReloadPanorama );
	::RegisterForUnhandledEvent( ToggleDebugger(), this, &CUIEngine::OnToggleDebug );
	::RegisterForUnhandledEvent( ShowPanelZoo(), this, &CUIEngine::OnShowPanelZoo );
	::RegisterForUnhandledEvent( DumpMemory(), this, &CUIEngine::OnMemDump );
#endif

	::RegisterForUnhandledEvent( ToggleConsole(), this, &CUIEngine::OnToggleConsole );
	::RegisterForUnhandledEvent( TopLevelWindowClose(), this, &CUIEngine::OnWindowShutdown );
	::RegisterForUnhandledEvent( DeletePanel(), this, &CUIEngine::OnDeletePanel );
	::RegisterForUnhandledEvent( SetInputFocus(), this, &CUIEngine::OnSetInputFocus );
	::RegisterForUnhandledEvent( DropInputFocus(), this, &CUIEngine::OnDropInputFocus );
	::RegisterForUnhandledEvent( CopyStringToClipboard(), this, &CUIEngine::OnCopyStringToClipboard );
	::RegisterForUnhandledEvent( ReloadStyleFile(), this, &CUIEngine::OnReloadStyleFile );
	::RegisterForUnhandledEvent( SetAllChildrenActivationEnabled(), this, &CUIEngine::OnSetAllChildrenActivationEnabled );
	::RegisterForUnhandledEvent( AsyncEvent(), this, &CUIEngine::OnAsyncEvent );
	::RegisterForUnhandledEvent( JSScheduledFunction(), this, &CUIEngine::OnJSScheduledFunction );
	::RegisterForUnhandledEvent( SetPanelSelected(), this, &CUIEngine::OnSetPanelSelected );
	::RegisterForUnhandledEvent( TogglePanelSelected(), this, &CUIEngine::OnTogglePanelSelected );
	::RegisterForUnhandledEvent( SetChildPanelsSelected(), this, &CUIEngine::OnSetChildPanelsSelected );

	::DispatchEventAsync( 0.2f, ReloadChangedUIFiles(), (IUIPanel*)NULL );

#if !defined( PANORAMA_DISABLE_VIDEO )
	VideoPlaybackInitialize();
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Run the UI loop until we get told to shutdown
//-----------------------------------------------------------------------------
void CUIEngine::Run()
{
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
	CVProfile *pProfile = GetVProfProfileForCurrentThread();
#endif
	while ( BIsRunning() )
	{
		// do NOT add any calls in here, they won't be tracked by our frame timer and 
		// will cause CSTime to drift if you do
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
		if ( pProfile )
			pProfile->MarkFrame( "UIEngine Main Thread" );
#endif
		RunFrame();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Will set the UI engine to aggressively limit frame rate it runs at to avoid resource usage
//-----------------------------------------------------------------------------
void CUIEngine::SetAggressiveFrameRateLimit( bool bLimitMainThread, bool bLimitRendering )
{
	m_bAggressivelyLimitFrameRate = bLimitMainThread;
	m_bAggressivelyLimitWindowFPS = bLimitRendering;
}


//-----------------------------------------------------------------------------
// Purpose: Add a callback object to listen for pre/post frame events
//-----------------------------------------------------------------------------
void CUIEngine::AddFrameListener( IUIEngineFrameListener *pListener )
{
	m_vecFrameListeners.AddToTail( pListener );
}


//-----------------------------------------------------------------------------
// Purpose: Remove a callback object to listen for pre/post frame events
//-----------------------------------------------------------------------------
void CUIEngine::RemoveFrameListener( IUIEngineFrameListener *pListener ) 
{
	m_vecFrameListeners.FindAndRemove( pListener );
}


//-----------------------------------------------------------------------------
// Purpose: Run scheduled delegates for this frame
//-----------------------------------------------------------------------------
void CUIEngine::RunScheduledDelegates()
{
	VPROF_BUDGET( "CUIEngine::RunScheduledDelegates", VPROF_BUDGETGROUP_TENFOOT );
	while( m_QueueScheduledDelegates.Count() )
	{
		const ScheduledItem_t item = m_QueueScheduledDelegates.ElementAtHead();
		if( item.m_flFrameTime > GetCurrentFrameTime() )
			break;
		else
		{
			m_QueueScheduledDelegates.RemoveAtHead();
			Assert( m_ListScheduledDelegates.IsValidIndex( item.m_iListIndex ) );
			CUtlDelegate< void() > &del = m_ListScheduledDelegates[item.m_iListIndex];
			if( !del.IsEmpty() )
			{
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
				CFmtStr strNodeName( "CUIEngine::RunScheduledDelegates %s", item.m_sName.String() );
				void *pScope = ((CUtlAbstractDelegate &)del.GetAbstractDelegate()).UnsafeGetThisPtr();
				if ( g_VProfile.IsNonNull() )
					g_VProfile->EnterScope( strNodeName, 0, VPROF_BUDGETGROUP_TENFOOT, false, pScope );
#endif

				del();

#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
				if ( g_VProfile.IsNonNull() )
					g_VProfile->ExitScope();
#endif
			}

			m_ListScheduledDelegates.Remove( item.m_iListIndex );
		}
	}
	m_flLastScheduledDelRunTime = m_flCurrentFrameTime;
}


//-----------------------------------------------------------------------------
// Purpose: Register a function to call each frame 
//-----------------------------------------------------------------------------
void CUIEngine::RegisterFrameFunc( PanoramaFrameFunc_t frameFunc )
{
	m_vecFrameFuncs.AddToTail( frameFunc );
}


//-----------------------------------------------------------------------------
// Purpose: Run any queued decrements on ref count objects, these are queued off main thread
// but on objects where we want to ensure destruction is on the main thread
//-----------------------------------------------------------------------------
void CUIEngine::RunQueuedDecRefCalls()
{
	CRefCount *pRefCount = NULL;
	while ( m_tslQueuedDecRef.PopItem(&pRefCount) && pRefCount != NULL )
	{
		pRefCount->Release();
		pRefCount = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Run a frame, pumps input, paints, whatever.
//-----------------------------------------------------------------------------
void CUIEngine::RunFrame()
{
	// Start timer
	m_fastTimerFrame.Start();

	EventStatsFrameUpdate();

	VPROF_BUDGET( "CUIEngine::RunFrame", VPROF_BUDGETGROUP_TENFOOT );

	UISoundSystem()->ServiceAudio();
#if !defined( SOURCE2_PANORAMA )
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
	ClientAPI_RunCallbacks();
#else
	SteamAPI_RunCallbacks();
#endif
#endif

	// Set that we haven't yet painted windows
	m_bPaintedWindows = false;

#if !defined( SOURCE2_PANORAMA )
	bool bSurfaceHasFocus = BAnyVisibleWindowHasFocus();
#endif

	// update time
	double flNewTime = Plat_FloatTime();
	if( flNewTime <= m_flCurrentFrameTime )
	{
		++m_ulFramesTimeWentBackward;

#if defined( SOURCE2_PANORAMA )
		bool bSurfaceHasFocus = BAnyVisibleWindowHasFocus();
#endif

		// Are we in fast (60fps target) or slow (16fps target) mode, should match below code to set limit
		if( !m_bAggressivelyLimitFrameRate || bSurfaceHasFocus )
		{
			if( m_ulFramesTimeWentBackward >= 5 )
			{
				AssertMsgOnce( false, "Time is going backwards or not moving, this is very bad." );
				m_ulFramesTimeWentBackward = 0;
				m_flCurrentFrameTime += 0.08333f;
			}
		}
		else
		{
			if( m_ulFramesTimeWentBackward >= 5 )
			{
				AssertMsgOnce( false, "Time is going backwards or not moving, this is very bad." );
				m_ulFramesTimeWentBackward = 0;
				m_flCurrentFrameTime += 0.3125f;
			}
		}
	}
	else
	{
		m_ulFramesTimeWentBackward = 0;
		m_flCurrentFrameTime = flNewTime;
	}

	// Limit animation frame rate
	CLimitTimer limit;

#if !defined( SOURCE2_PANORAMA )
	// Unnecessary in Source 2
	if( !bSurfaceHasFocus )
	{
		UISoundSystem()->ConsiderPausingAudio();
	}
#endif

	m_bWorkRemaining = false;

#if defined( SOURCE2_PANORAMA )
	limit.SetLimit( 5000 ); // Try to avoid using over 5ms for panorama ever in source2!
#else
	if( !m_bAggressivelyLimitFrameRate || bSurfaceHasFocus )
		limit.SetLimit( 16666 ); // 60hz
	else
		limit.SetLimit( 62500 ); // 16hz
#endif

	// Service the native V8 platform's foreground jobs on its owning thread.
	{
		v8::Isolate::Scope isolate_scope( m_pV8Isolate );
		while ( !limit.BLimitReached() && v8::platform::PumpMessageLoop( s_pNativeV8Platform, m_pV8Isolate ) ) {}
	}

	bool bFirstIteration = true;

#if !defined( SOURCE2_PANORAMA_FIXME )
	CRTime::UpdateRealTime();
#endif

	FOR_EACH_VEC( m_vecFrameListeners, i )
	{
		m_vecFrameListeners[i]->OnPreFrame();
	}

	// let chrome think
#if !defined( SOURCE2_PANORAMA )
	if ( m_pHTMLController )
	{
		if ( !m_bAggressivelyLimitFrameRate || bSurfaceHasFocus )
		{
			m_pHTMLController->SetCefThreadTargetFrameRate( 60 );
		}
		else
		{
			m_pHTMLController->SetCefThreadTargetFrameRate( 10 );
		}
		m_pHTMLController->RunFrame();
	}
#endif

#if 0 // Debugging aid
	static float64 s_lastV8Stats;
	if ( m_pV8Isolate &&
		 Plat_FloatTime() - s_lastV8Stats > 5 )
	{
		v8::HeapStatistics v8Stats;

		m_pV8Isolate->GetHeapStatistics( &v8Stats );
		Msg( "v8 total heap size %llu KB, used %llu KB of limit %llu KB\n",
			 (uint64)v8Stats.total_heap_size() / 1024,
			 (uint64)v8Stats.used_heap_size() / 1024,
			 (uint64)v8Stats.heap_size_limit() / 1024 );
		Msg( "   total executable %llu KB, total physical %llu KB\n",
			 (uint64)v8Stats.total_heap_size_executable() / 1024,
			 (uint64)v8Stats.total_physical_size() / 1024 );
		
		s_lastV8Stats = Plat_FloatTime();
	}
#endif

	// In source 2 we aren't sleeping/controlling frame rate so we don't want to try to complete extra work 
	// if we have time left, we just want to be done with the frame and do more next frame as soon as we've done
	// the minimum to complete a full frame.
#if !defined( SOURCE2_PANORAMA )
	while( 1 )
#endif
	{
		UISoundSystem()->ServiceAudio();

		if( bFirstIteration )
		{
			FOR_EACH_VEC( m_vecFrameFuncs, i )
			{
				m_vecFrameFuncs.Element(i)();
			}
		}

		bFirstIteration = false;

		{
			VPROF_BUDGET( "CUIEngine::RunFrame subframe loop", VPROF_BUDGETGROUP_TENFOOT );
			m_pFileSystem->RunFrame();
		}

		if( BIsRunning() )
		{
			// Run any platform specific frame funcs, implemented on child classes
			DispatchQueuedEvent( limit );

			RunScheduledDelegates();

			RunPlatformFrame();

			if( !m_bPaintedWindows )
				LayoutAndPaintWindows();

#if V8_DEBUGGING_ENABLED

			DebuggerRunFrame();
#endif
		}

		if( m_bDoV8GarbageCollect )
		{
			VPROF_BUDGET( "V8 - Hint GC Full", VPROF_BUDGETGROUP_TENFOOT );
			// Garbage collect a bit
			v8::Isolate::Scope isolate_scope( m_pV8Isolate );
			if( m_pV8Isolate->IdleNotificationDeadline( s_pNativeV8Platform->MonotonicallyIncreasingTime() + 0.1 ) )
				m_bDoV8GarbageCollect = false;
			else
				m_bWorkRemaining = true;

			m_flLastV8IncrementalGC = GetCurrentFrameTime();
		}

#if !defined( SOURCE2_PANORAMA )
		// If no work was left, or if we've reaced our limit break out to end this frame
		if ( !m_bWorkRemaining || limit.BLimitReached() )
			break;
#endif
	}

#if !defined( PANORAMA_DISABLE_VIDEO )
	VideoPlaybackRunFrame();
#endif

	FOR_EACH_VEC( m_vecFrameListeners, i )
	{
		m_vecFrameListeners[i]->OnPostFrame();
	}

	RunQueuedDecRefCalls();

#if defined(PANORAMA_USE_S1WRAPPER)
	g_pResourceSystem->Update();
#endif

	// Do smaller incremental v8 GC hinting if we haven't done it for a long time
	if( GetCurrentFrameTime() - m_flLastV8IncrementalGC > 1.0f && !limit.BLimitReached() )
	{
		VPROF_BUDGET( "V8 - Hint GC Periodic", VPROF_BUDGETGROUP_TENFOOT );
		v8::Isolate::Scope isolate_scope( m_pV8Isolate );
		m_pV8Isolate->IdleNotificationDeadline( s_pNativeV8Platform->MonotonicallyIncreasingTime() + 0.001 );
		m_flLastV8IncrementalGC = GetCurrentFrameTime();
	}

	// In source 2 panorama shouldn't sleep, it doesn't control framerate! Finish a frame as fast as we can!
#if !defined( SOURCE2_PANORAMA )
	// If we didn't reach the limit sleep a bit to limit frame rate, otherwise always sleep 
	// at least a millisecond to yield to other frames anyway.
	{
		m_eventPaintThread.Reset(); // we just did work, only wait if our buffer is cleared after now
		VPROF_BUDGET( "Sleep - FPS Limiting", VPROF_BUDGETGROUP_TENFOOT );

		int nSleepCount = 0;
		while( !limit.BLimitReached() && nSleepCount < 5 )
		{
			m_eventPaintThread.Wait( limit.CMicroSecLeft() / 1000 );
			++nSleepCount;
		}
		
		if( nSleepCount == 0 )
			m_eventPaintThread.Wait( 1 );
	}
#endif 

	// Update times for next frame
	m_fastTimerFrame.End();

#if !defined( SOURCE2_PANORAMA_FIXME )
	CSTime::UpdateServerTime( m_fastTimerFrame.GetDuration().GetMicroseconds() );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Platform frame func
//-----------------------------------------------------------------------------
void CUIEngine::RunPlatformFrame()
{
	RunControllerFrame();
}


//-----------------------------------------------------------------------------
// Purpose: Create sound system
//-----------------------------------------------------------------------------
IUISoundSystem *CUIEngine::CreateSoundSystem()
{
	return new CUISoundSystem();
}


//-----------------------------------------------------------------------------
// Purpose: return true if any of our windows has foreground window focus
//-----------------------------------------------------------------------------
bool CUIEngine::BHasFocus()
{
	FOR_EACH_VEC( m_vecWindows, i )
	{
		if ( m_vecWindows[i]->BHasFocus() )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Turn on paint count tracking for panels
//-----------------------------------------------------------------------------
void CUIEngine::SetPaintCountTrackingEnabled( bool bEnablePaintCountTracking )
{
	AUTO_LOCK( m_MutexPanelPaintCounts );
	m_bPaintCountTrackingEnabled = bEnablePaintCountTracking;
	m_MapPanelPaintCounts.Purge();
	m_unMaxPanelPaintsSinceReset = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Increment paint count tracking for panels
//-----------------------------------------------------------------------------
void CUIEngine::IncrementPaintCountForPanel( uint64 ulPanelPtrValue, bool bRequiredCompositionLayer, double flFrameTime )
{
	if( m_bPaintCountTrackingEnabled )
	{
		AUTO_LOCK( m_MutexPanelPaintCounts );
		int iMap = m_MapPanelPaintCounts.Find( ulPanelPtrValue );
		if( iMap == m_MapPanelPaintCounts.InvalidIndex() )
			iMap = m_MapPanelPaintCounts.Insert( ulPanelPtrValue );

		m_MapPanelPaintCounts[iMap].m_unPaintsSinceReset++;
		if ( m_MapPanelPaintCounts[iMap].m_unPaintsSinceReset > m_unMaxPanelPaintsSinceReset )
		{
			m_unMaxPanelPaintsSinceReset = m_MapPanelPaintCounts[iMap].m_unPaintsSinceReset;
		}
		m_MapPanelPaintCounts[iMap].m_bLastNeededCompositionLayer = bRequiredCompositionLayer;
		m_MapPanelPaintCounts[iMap].m_flLastPaintTime = flFrameTime;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get panel paint info for the panel
//-----------------------------------------------------------------------------
void CUIEngine::GetPanelPaintInfo( uint64 ulPanelPtrValue, uint32 &unMaxPanelPaintCount, uint32 &unPaintCount, bool &bRequiredCompositionLayer, double &flFrameTimeLastPaint )
{
	AUTO_LOCK( m_MutexPanelPaintCounts );
	unMaxPanelPaintCount = m_unMaxPanelPaintsSinceReset;
	int iMap = m_MapPanelPaintCounts.Find( ulPanelPtrValue );
	if( iMap == m_MapPanelPaintCounts.InvalidIndex() )
	{
		unPaintCount = 0;
		bRequiredCompositionLayer = false;
		flFrameTimeLastPaint = 0.0;
	}
	else
	{
		unPaintCount = m_MapPanelPaintCounts[iMap].m_unPaintsSinceReset;
		bRequiredCompositionLayer = m_MapPanelPaintCounts[iMap].m_bLastNeededCompositionLayer;
		flFrameTimeLastPaint = m_MapPanelPaintCounts[iMap].m_flLastPaintTime;
	}
}

//-----------------------------------------------------------------------------
// Purpose:  Determine position for haptic feedback
//-----------------------------------------------------------------------------
IUIEngine::EHapticFeedbackPosition CUIEngine::GetHapticFeedbackPositionForInteraction()
{
	return UIInputEngine()->GetHapticFeedbackPositionForInteraction();
}

//-----------------------------------------------------------------------------
// Purpose:  Pulse haptic feedback
//-----------------------------------------------------------------------------
void CUIEngine::PulseActiveControllerHaptic( IUIEngine::EHapticFeedbackPosition ePosition, IUIEngine::EHapticFeedbackStrength eStrength )
{
	UIInputEngine()->PulseActiveControllerHaptic( ePosition, eStrength );
}


//-----------------------------------------------------------------------------
// Purpose: Mark a layer to repaint
//-----------------------------------------------------------------------------
void CUIEngine::MarkLayerToRepaintThreadSafe( uint64 ulCompositionLayerID )
{
	AUTO_LOCK( m_MutexLayersToRepaint );
	m_treeLayersToRepaint.Insert( ulCompositionLayerID );
}

//-----------------------------------------------------------------------------
// Purpose: Paints a frame, may not actually finish and swap buffers if animation/render 
// threads are blocked.
//
// Important to be able to keep the main thread running in those cases so the system message pump
// doesn't stall and make us look dead, or cause a deadlock with the render thread (which is possible on
// windows due to DXGI suckiness using SendMessage()).
//-----------------------------------------------------------------------------
void CUIEngine::LayoutAndPaintWindows()
{
	VPROF_BUDGET( "CUIEngine::PaintWindows", VPROF_BUDGETGROUP_TENFOOT );
	
	{
		VPROF_BUDGET( "CUIEngine::PaintWindows - Requeue paint", VPROF_BUDGETGROUP_TENFOOT );
		AUTO_LOCK( m_MutexLayersToRepaint );
		FOR_EACH_RBTREE_FAST( m_treeLayersToRepaint, i )
		{
			CPanelPtr< IUIPanel > ptr;
			ptr.SetFromUInt64( m_treeLayersToRepaint[i] );

			if ( ptr.Get() )
			{
				//Msg( "Queueing repaint of %s %s\n", ptr->GetID(), ptr->ClientPtr()->GetPanelType().String() );
				ptr->SetRepaint( k_EPanelRepaintFull );
			}
		}
		m_treeLayersToRepaint.RemoveAll();
	}

	CallQueuedPanelsBeforeStyleAndLayout();
	m_bPaintedWindows = true;	

	// First, if there is a fullscreen window, only it should be painted, others will block up unable to present.
	CTopLevelWindow *pFirstFullscreen = NULL;
	CTopLevelWindow *pFocusedOverlayWindow = NULL;
#if !defined( SOURCE2_PANORAMA ) 
	FOR_EACH_VEC( m_vecWindows, i )
	{
		CTopLevelWindow *pWindow = m_vecWindows[i];
		if ( !pFirstFullscreen && pWindow->BIsVisible() && pWindow->BIsFullscreen() )
		{
			pFirstFullscreen = pWindow;
		}

		if ( !pFocusedOverlayWindow && pWindow->BHasFocus() && pWindow->BIsVisible() && pWindow->BIsOverlay() )
		{
			pFocusedOverlayWindow = pWindow;
		}
	}
#endif


	if ( pFirstFullscreen )
	{
		pFirstFullscreen->LayoutAndPaintIfNeeded();
	}
	else
	{
		const float k_flMinMaxFPS = 5.0f;
		// Focus window goes first
		if ( pFocusedOverlayWindow )
		{
			pFocusedOverlayWindow->SetMaxFPS( MAX( s_convarMaxOverlayFPS.GetFloat(), k_flMinMaxFPS ) );
			pFocusedOverlayWindow->LayoutAndPaintIfNeeded();
		}


		float flMaxFPSWhenThrottled = MAX( s_convarOutOfFocusMaxFPS.GetFloat(), k_flMinMaxFPS );
		FOR_EACH_VEC( m_vecWindows, i )
		{
			CTopLevelWindow *pWindow = m_vecWindows[i];
			if ( pFocusedOverlayWindow )
			{
				if ( pWindow != pFocusedOverlayWindow && pWindow->BIsVisible() )
				{
					pWindow->SetMaxFPS( flMaxFPSWhenThrottled );
					pWindow->LayoutAndPaintIfNeeded();
				}
			}
			else
			{
				if ( pWindow->BIsVisible() )
				{
					if( !pWindow->BDeviceLost() )
					{
						if( pWindow->BIsOverlay() )
							pWindow->SetMaxFPS( MAX( s_convarMaxOverlayFPS.GetFloat(), k_flMinMaxFPS ) );
						else
						{
							if( m_bAggressivelyLimitWindowFPS )
								pWindow->SetMaxFPS( flMaxFPSWhenThrottled );
							else
								pWindow->SetMaxFPS( MAX( s_convarMaxFPS.GetFloat(), k_flMinMaxFPS ) );
						}

						pWindow->LayoutAndPaintIfNeeded();
					}
				}
				else
				{
#if defined( SOURCE2_PANORAMA )
					pWindow->PaintEmptyFrameAndForceLaterRepaint();
#endif
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets last input time to current frame time
//-----------------------------------------------------------------------------
void CUIEngine::UpdateLastInputTime()
{
	m_flLastInputTime = GetCurrentFrameTime();
	float flTimeSinceLastInput = m_flLastInputTime - m_flLastUserActiveReportTime;
	if ( flTimeSinceLastInput > 1.0f )
	{
		m_flLastUserActiveReportTime = m_flLastInputTime;
		DispatchEvent( UserInputActive::MakeEvent( NULL ) ); // tell anyone listening that the user is typing
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a panel to our queue of panels that need to be called before style & layout pass
//-----------------------------------------------------------------------------
void CUIEngine::CallBeforeStyleAndLayout( IUIPanel *pPanel )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	if ( m_treeCallBeforeStyleAndLayout.Find( pPanel ) == m_treeCallBeforeStyleAndLayout.InvalidIndex() )
		m_treeCallBeforeStyleAndLayout.Insert( ptrPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Calls all registered panels
//-----------------------------------------------------------------------------
void CUIEngine::CallQueuedPanelsBeforeStyleAndLayout()
{
	CUtlRBTree< CPanelPtr< IUIPanel >, int, CDefLess< CPanelPtr< IUIPanel > > > treeTemp;
	treeTemp.Swap( m_treeCallBeforeStyleAndLayout );

	FOR_EACH_RBTREE_FAST( treeTemp, i )
	{
		IUIPanel *pPanel = treeTemp.Element( i ).Get();
		if ( pPanel )
			pPanel->ClientPtr()->OnCallBeforeStyleAndLayout();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tells all panels to reload the specified layout file if needed
//-----------------------------------------------------------------------------
void CUIEngine::ReloadLayoutFile( CPanoramaSymbol symPath )
{
	FOR_EACH_VEC( m_vecWindows, i )
	{
		m_vecWindows[i]->ReloadLayoutFile( symPath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Toggles in and out of debug mode
//-----------------------------------------------------------------------------
void CUIEngine::ToggleDebugMode()
{
	CreateDebuggerWindow();
	::DispatchEvent( ::BeginDebuggerInspect(), (IUIPanel*)NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Creates our debugger window
//-----------------------------------------------------------------------------
void CUIEngine::CreateDebuggerWindow()
{	
	if( m_bDebuggerActive )
		return;

	m_bDebuggerActive = true;
	::DispatchEvent( ::CreateDebuggerWindow(), (IUIPanel*)NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Called when the debugger window is closing
//-----------------------------------------------------------------------------
void CUIEngine::CloseDebuggerWindow()
{	
	if( !m_bDebuggerActive )
		return;

	m_bDebuggerActive = false;
	::DispatchEvent( ::CloseDebuggerWindow(), (IUIPanel*)NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a window 
//-----------------------------------------------------------------------------
void CUIEngine::CreatePanelZooWindow()
{
	/*
	m_pPanelZooWindow = CreateNewWindow ( "Framework Test", 1920, 800, false, false, false, false, "" );
	new CBackgroundImageTest( m_pPanelZooWindow, "FrameworkTest" );
	*/
}


//-----------------------------------------------------------------------------
// Purpose: Toggles in and out of debug mode
//-----------------------------------------------------------------------------
bool CUIEngine::OnToggleConsole()
{
	if ( m_pConsoleWindow )
	{
		m_pConsoleWindow->Delete();
		m_pConsoleWindow = NULL;
	}
	else
	{
		CreateConsoleWindow();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates our debugger window
//-----------------------------------------------------------------------------
void CUIEngine::CreateConsoleWindow()
{	
	//m_pConsoleWindow = CreateNewWindow( "Steam Console", 1280, 720, k_ERenderToWindow, false, false, false, "" );

	// top level window will take ownership of the panel
	//new CConsole( m_pConsoleWindow, "Console" );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel is created
//-----------------------------------------------------------------------------
IUIPanel * CUIEngine::CreatePanel( IUIWindow *pWindow )
{
	if ( !pWindow )
	{
		AssertMsg( false, "A panel must be created within a window." );
		return nullptr;
	}

	CUIPanel *pPanel = ( ( CTopLevelWindow * )pWindow )->CreatePanel();

	// insert the panel into our map, generating a serial number
	int iMap = m_mapPanels.Find( pPanel );	
	if ( iMap == m_mapPanels.InvalidIndex() )
	{
		// let serial number overflow, should be ok
		m_mapPanels.Insert( pPanel, m_unPanelSerialNumber++ );
	}
	else
	{
		AssertMsg( false, "PanelCreated called twice for the same panel pointer" );
	}

	// tell the debugger if active
	//if ( m_pDebugger )
	//	m_pDebugger->OnPanelAdded( pPanel );

	return pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel is reloading layout and needs javascript context 
// fully recreated, or when the panel is actually being destroyed and we should 
// free the context
//-----------------------------------------------------------------------------
void CUIEngine::DeleteScriptContext( IUIPanel *pPanelContext )
{
	// Remove any js events associated with this panel (added by calling $.RegisterEventHandler)
	// to avoid duplicate registration on reload
	VecEventHandlers_t *pvec = ( (CUIPanel*)pPanelContext )->GetMutableEventHandlers();
	FOR_EACH_VEC_BACK( *pvec, i )
	{
		EventHandler_t handler = pvec->Element( i );
		// Only removing javascript event handlers
		if ( handler.pjsHandler )
		{
			DecrementEventHandlerCount( handler.symEvent, false );
			handler.pjsHandler->Reset();
			delete handler.pjsHandler;

			pvec->FastRemove( i );	// valid as iterating backwards
		}
	}
	
	// Check that this panel has no unhandled event handlers
	int iMapEvent = m_mapPanelToJSUnhandledEventHandlers.Find( pPanelContext );
	if ( iMapEvent != m_mapPanelToJSUnhandledEventHandlers.InvalidIndex() )
	{
		VecEventHandlers_t *pvecHandlers = m_mapPanelToJSUnhandledEventHandlers.Element( iMapEvent );

		// Create a copy of the events to unregister first so that our
		// vector doesn't change from under us while unregistering.
		VecEventHandlers_t vecEventsToUnregister;
		FOR_EACH_VEC( *pvecHandlers, iVec )
		{
			vecEventsToUnregister.AddToTail( pvecHandlers->Element( iVec ) );
		}

		FOR_EACH_VEC( vecEventsToUnregister, iVec )
		{
			EventHandler_t ev = vecEventsToUnregister[iVec];
			UnregisterJSForUnhandledEvent( ev.symEvent, ev.unHandlerId );
		}
	}

	// Remove any generic callbacks associated with this panel
	int iMapGenericCallbacks = m_mapPanelToJSGenericCallbacks.Find( pPanelContext );
	if ( iMapGenericCallbacks != m_mapPanelToJSGenericCallbacks.InvalidIndex() )
	{
		VecJSGenericCallbackPtr_t *pvecCallbacks = m_mapPanelToJSGenericCallbacks.Element( iMapGenericCallbacks );
		CUtlVector<JSGenericCallbackHandle_t> callbacksToUnregister;

		FOR_EACH_VEC( *pvecCallbacks, iCallback )
		{
			callbacksToUnregister.AddToTail( pvecCallbacks->Element( iCallback )->m_nCallbackHandle );
		}

		FOR_EACH_VEC( callbacksToUnregister, iCallback )
		{
			UnregisterJSGenericCallback( callbacksToUnregister[ iCallback ] );
		}
	}

	int iV8Map = m_MapPanelV8Contexts.Find( pPanelContext );
	if( iV8Map != m_MapPanelV8Contexts.InvalidIndex() )
	{
		m_mapOtherPanelsV8InContext.Remove( pPanelContext );

		v8::Isolate::Scope isolate_scope( m_pV8Isolate );
		v8::HandleScope handle_scope( m_pV8Isolate );

#if V8_DEBUGGING_ENABLED

		if ( m_pInspectorClient )
		{
			m_pInspectorClient->ContextDestroyed( m_MapPanelV8Contexts[iV8Map]->Get( m_pV8Isolate ) );
		}

#endif

		m_MapPanelV8Contexts[iV8Map]->Reset();
		delete m_MapPanelV8Contexts[iV8Map];
		m_MapPanelV8Contexts.RemoveAt( iV8Map );

		m_pV8Isolate->ContextDisposedNotification();

		m_bDoV8GarbageCollect = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel is destroyed
//-----------------------------------------------------------------------------
void CUIEngine::PanelDestroyed( IUIPanel *pPanel, IUIPanel *pOldParent )
{
	int iV8Map = m_MapV8PanelObjectInstances.Find( pPanel );
	if( iV8Map != m_MapV8PanelObjectInstances.InvalidIndex() )
	{
		v8::Isolate::Scope isolate_scope( m_pV8Isolate );
		v8::HandleScope handle_scope( m_pV8Isolate );

		v8::Persistent<v8::Object> * pPersistent = m_MapV8PanelObjectInstances.Element( iV8Map );

		v8::Local<v8::Object> obj = v8::Local<v8::Object>::New( UIEngineInternal()->GetV8Isolate(), *(pPersistent) );
		obj->SetInternalField( 0, v8::External::New( UIEngineInternal()->GetV8Isolate(), NULL ) );
		pPersistent->Reset();
		delete pPersistent;
		m_MapV8PanelObjectInstances.RemoveAt( iV8Map );
	}

	iV8Map = m_MapV8PanelStyleObjectInstances.Find( pPanel );
	if( iV8Map != m_MapV8PanelStyleObjectInstances.InvalidIndex() )
	{
		v8::Isolate::Scope isolate_scope( m_pV8Isolate );
		v8::HandleScope handle_scope( m_pV8Isolate );

		v8::Persistent<v8::Object> * pPersistent = m_MapV8PanelStyleObjectInstances.Element( iV8Map );

		v8::Local<v8::Object> obj = v8::Local<v8::Object>::New( GetV8Isolate(), *(pPersistent) );
		obj->SetInternalField( 0, v8::External::New( GetV8Isolate(), NULL ) );
		pPersistent->Reset();
		delete pPersistent;
		m_MapV8PanelStyleObjectInstances.RemoveAt( iV8Map );
	}

	// tell listeners
	FOR_EACH_VEC( m_vecPanelDestroyedDelegates, i )
	{
		m_vecPanelDestroyedDelegates[i]( pPanel, pOldParent );
	}

	DeleteScriptContext( pPanel );

	// Remove from tree of pending deletes
	m_treePanelsWaitingAsyncDelete.Remove( pPanel );

	// clean up any strings they alloc'd
	m_pLocalization->OnPanelDeleted( pPanel );

	// tell the debugger if active
	//if ( m_pDebugger )
	//	m_pDebugger->OnPanelDeleted( pPanel, pOldParent );

	// remove from our map
	int iMap = m_mapPanels.Find( pPanel );
	if ( iMap != m_mapPanels.InvalidIndex() )
	{
		m_mapPanels.RemoveAt( iMap );
	}
	else
	{
		AssertMsg( false, "PanelDestroyed called for a panel which does not exist" );
	}

	// Time to actually delete framework part of panel
	CUIPanel *pUIPanel = (CUIPanel*)pPanel;
	IUIWindow *pWindow = pUIPanel->GetParentWindow();
	if ( pWindow )
	{
		( ( CTopLevelWindow * )pWindow )->FreePanel( pUIPanel );
	}
	else
	{
		AssertMsg( false, "Can't delete a panel not contained within a window." );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Allows callers to register to be called when a panel is destroyed
//-----------------------------------------------------------------------------
void CUIEngine::RegisterForPanelDestroyed( PanelDestroyedDel_t del )
{
	Assert( !m_vecPanelDestroyedDelegates.HasElement( del ) );
	m_vecPanelDestroyedDelegates.AddToTail( del );
}


//-----------------------------------------------------------------------------
// Purpose: Allows callers to remove registration for when a panel is destroyed
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterForPanelDestroyed( PanelDestroyedDel_t del )
{
	m_vecPanelDestroyedDelegates.FindAndFastRemove( del );
}


//-----------------------------------------------------------------------------
// Purpose: Track panels with queued async delete
//-----------------------------------------------------------------------------
void CUIEngine::SetPanelWaitingAsyncDelete( IUIPanel *pPanel )
{
	if ( m_treePanelsWaitingAsyncDelete.Find( pPanel ) == m_treePanelsWaitingAsyncDelete.InvalidIndex() )
		m_treePanelsWaitingAsyncDelete.Insert( pPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Check if panel while still existing may be in a state where it is definitely going to be deleted
//-----------------------------------------------------------------------------
bool CUIEngine::BIsPanelWaitingAsyncDelete( IUIPanel *pPanel )
{
	if ( m_treePanelsWaitingAsyncDelete.Find( pPanel ) != m_treePanelsWaitingAsyncDelete.InvalidIndex() )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to delete a panel. Usually fired async
//-----------------------------------------------------------------------------
bool CUIEngine::OnDeletePanel( const CPanelPtr< IUIPanel > &pPanel )
{
	IUIPanel *ptr = pPanel.Get();
	if ( ptr )
	{
		ptr->ClientPtr()->OnDeletePanel();
		m_treePanelsWaitingAsyncDelete.Remove( ptr );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Copy string to clipboard
//-----------------------------------------------------------------------------
bool CUIEngine::OnCopyStringToClipboard( const CPanelPtr< IUIPanel > &ptrPanel, const char *pchString, const char *pszPasteLocToken )
{
	CopyToClipboard( pchString, pszPasteLocToken );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Tells all panels to reload their styles
//-----------------------------------------------------------------------------
bool CUIEngine::OnReloadStyleFile( CPanoramaSymbol symFile )
{
	FOR_EACH_VEC( m_vecWindows, i )
	{
		m_vecWindows[i]->OnReloadStyleFile( symFile );
	}

	// let bubble to other callers
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to set focus
//-----------------------------------------------------------------------------
bool CUIEngine::OnSetInputFocus( const CPanelPtr< IUIPanel > &ptrPanel )
{
	IUIPanel *pPanel = ptrPanel.Get();
	if ( pPanel )
		pPanel->SetFocus();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles event to drop focus
//-----------------------------------------------------------------------------
bool CUIEngine::OnDropInputFocus( const CPanelPtr< IUIPanel > &ptrPanel )
{
	IUIPanel *pPanel = ptrPanel.Get();
	if ( pPanel )
	{
		IUIWindow *pWindow = pPanel->GetParentWindow();
		if ( pWindow )
		{
			pWindow->UIWindowInput()->SetInputFocus( nullptr, false, false );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles event to set selection
//-----------------------------------------------------------------------------
bool CUIEngine::OnSetPanelSelected( const CPanelPtr< IUIPanel > &ptrPanel, bool bSelected )
{
	IUIPanel *pPanel = ptrPanel.Get();
	if ( pPanel )
		pPanel->SetSelected( bSelected );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles event to toggle selection
//-----------------------------------------------------------------------------
bool CUIEngine::OnTogglePanelSelected( const CPanelPtr< IUIPanel > &ptrPanel )
{
	IUIPanel *pPanel = ptrPanel.Get();
	if ( pPanel )
		pPanel->SetSelected( !pPanel->IsSelected() );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to set child selection
//-----------------------------------------------------------------------------
bool CUIEngine::OnSetChildPanelsSelected( const CPanelPtr< IUIPanel > &ptrPanel, bool bSelected )
{
	IUIPanel *pPanel = ptrPanel.Get();
	if ( pPanel )
	{
		int nChildCount = pPanel->GetChildCount();
		for ( int i = 0; i < nChildCount; ++i )
		{
			IUIPanel *pChild = pPanel->GetChild( i );
			pChild->SetSelected( bSelected );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to set focus
//-----------------------------------------------------------------------------
bool CUIEngine::OnAsyncEvent( float flDelay, IUIEvent * pEvent )
{
	DispatchEventAsync( flDelay, pEvent->Copy() );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to set all a panels children enabled/disabled
//-----------------------------------------------------------------------------
bool CUIEngine::OnSetAllChildrenActivationEnabled( const CPanelPtr< IUIPanel > &ptrPanel, bool bEnabled )
{
	IUIPanel *pPanel = ptrPanel.Get();
	if ( pPanel )
	{
		pPanel->SetAllChildrenActivationEnabled( bEnabled );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if a raw panel pointer is valid
//-----------------------------------------------------------------------------
bool CUIEngine::IsValidPanelPointer( const IUIPanel *pPanel )
{
	return m_mapPanels.Find( (IUIPanel*)pPanel ) != m_mapPanels.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Purpose: Looks up a panel handle for the listed panel
//-----------------------------------------------------------------------------
PanelHandle_t CUIEngine::GetPanelHandle( const IUIPanel *pPanel )
{
	int iMap = m_mapPanels.Find( (IUIPanel *)pPanel );
	
	// as all panels should be tracked from construction to destruction, hitting the case below means someone passed in a garbage pointer!
	if ( iMap == m_mapPanels.InvalidIndex() )
	{
		AssertMsg( false, "Trying to retrieve a panel handle for an invalid panel" );
		PanelHandle_t handle = { m_mapPanels.InvalidIndex(), 0 };
		return handle;
	}

	PanelHandle_t ret = { iMap, m_mapPanels.Element( iMap ) };
	return ret;
}


//-----------------------------------------------------------------------------
// Purpose: Looks up a panel handle for the listed panel
//			Returns NULL if panel no longer exists
//-----------------------------------------------------------------------------
IUIPanel *CUIEngine::GetPanelPtr( const PanelHandle_t &handle )
{
	if ( !m_mapPanels.IsValidIndex( handle.m_iPanelIndex ) )
		return NULL;

	// make sure serial number matches or index could be taken by a new panel
	if ( m_mapPanels.Element( handle.m_iPanelIndex ) != handle.m_unSerialNumber )
		return NULL;

	return m_mapPanels.Key( handle.m_iPanelIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Called to alloc panel style
//-----------------------------------------------------------------------------
IUIPanelStyle *CUIEngine::AllocPanelStyle( IUIPanel *pPanel )
{
	CPanelStyle *pStyle = (CPanelStyle*)m_PanelStylePool.Alloc();
	ConstructOneArg( pStyle, pPanel );
	return pStyle;
}


//-----------------------------------------------------------------------------
// Purpose: Called to free panel style
//-----------------------------------------------------------------------------
void CUIEngine::FreePanelStyle( IUIPanelStyle * pStyle )
{
	CPanelStyle *pCStyle = (CPanelStyle*)pStyle;
	Destruct( pCStyle );
	m_PanelStylePool.Free( pCStyle );
}


//-----------------------------------------------------------------------------
// Purpose: Called when toggling debug state is requested
//-----------------------------------------------------------------------------
bool CUIEngine::OnToggleDebug()
{	
	ToggleDebugMode();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we need to show the panel zoo window
//-----------------------------------------------------------------------------
bool CUIEngine::OnShowPanelZoo()
{
	if ( !m_pPanelZooWindow )
		CreatePanelZooWindow();

	if ( m_pPanelZooWindow )
		m_pPanelZooWindow->Activate( false );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when dumping memory is requested
//-----------------------------------------------------------------------------
bool CUIEngine::OnMemDump()
{

#if !defined( NO_MALLOC_OVERRIDE ) && !defined( SOURCE2_PANORAMA )
	g_pMemAllocSteam->DumpStats();
#endif
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if any handler is registered for an event
//-----------------------------------------------------------------------------
bool CUIEngine::BAnyHandlerRegisteredForEvent( const CPanoramaSymbol &symEvent )
{
	int iMap = m_mapEventsToHandlerCounts.Find( symEvent );
	if ( iMap == m_mapEventsToHandlerCounts.InvalidIndex() )
		return false;
	else 
	{
		HandlerCount_t &counts = m_mapEventsToHandlerCounts[iMap];
		return counts.m_nPanelHandlers > 0 || counts.m_nUnhandledHandlers > 0 || counts.m_nPanelTypeHandlers > 0 ? true : false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Have base handlers for panel2d been initialized?
//-----------------------------------------------------------------------------
bool CUIEngine::BHaveEventHandlersRegisteredForType( CPanoramaSymbol symPanelType )
{
	if( m_mapPanelTypeEventHandlers.Find( symPanelType ) != m_mapPanelTypeEventHandlers.InvalidIndex() )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Set a base handler registration for all panel2d objects
//-----------------------------------------------------------------------------
void CUIEngine::RegisterPanelTypeEventHandler( CPanoramaSymbol symMsg, CPanoramaSymbol symPanelType, CUtlAbstractDelegate pFunc, bool bThisPtrIsUIPanel /* = false */ )
{
#if defined( SOURCE2_PANORAMA )
	typedef HashMapFunctor_t< panorama::CPanoramaSymbol > PanoramaSymbolHashFunctor;
#else
	typedef HashFunctor< panorama::CPanoramaSymbol > PanoramaSymbolHashFunctor;
#endif

	int iMap = m_mapPanelTypeEventHandlers.Find( symPanelType );
	if( iMap == m_mapPanelTypeEventHandlers.InvalidIndex() )
		iMap = m_mapPanelTypeEventHandlers.Insert( symPanelType, new CUtlHashMap< panorama::CPanoramaSymbol, PanelTypeEventHandler_t, CDefEquals< panorama::CPanoramaSymbol >, PanoramaSymbolHashFunctor >() );

	PanelTypeEventHandler_t handler;
	handler.del = pFunc;
	handler.m_bIsUIPanelThisPtr = bThisPtrIsUIPanel;
	m_mapPanelTypeEventHandlers[iMap]->Insert( symMsg, handler );
	IncrementEventHandlerCount( symMsg, false, true );
}


//-----------------------------------------------------------------------------
// Purpose: Registers a panel for listening to messages
//-----------------------------------------------------------------------------
void CUIEngine::RegisterEventHandler( CPanoramaSymbol symEvent, IUIPanel *pPanel, CUtlAbstractDelegate pFunc )
{
	VPROF_BUDGET( "CUIEngine::RegisterEventHandler", VPROF_BUDGETGROUP_TENFOOT );

	if ( !pPanel )
	{
		Assert( "Failed to find event handlers for panel" );
		return;
	}

	VecEventHandlers_t *pvec = ((CUIPanel*)pPanel)->GetMutableEventHandlers();
	EventHandler_t handler;
	handler.symEvent = symEvent;
	handler.pHandler = pFunc;
	handler.pjsHandler = NULL;

#ifdef _DEBUG
	FOR_EACH_VEC( *pvec, i )
	{
		EventHandler_t &existingHandler = pvec->Element( i );
		if( existingHandler.symEvent == symEvent && existingHandler.pHandler.IsEqual( pFunc ) )
		{
			AssertMsg( false, "Event handler already registered for this panel" );
			return;
		}
	}
#endif

	IncrementEventHandlerCount( symEvent, false );

	pvec->AddToTail( handler );
}

//-----------------------------------------------------------------------------
// Purpose: Event handler helper
//-----------------------------------------------------------------------------
void CUIEngine::ValidateJSFunction( v8::Persistent< v8::Function > *pFunc )
{
	v8::Isolate::Scope isolate_scope(m_pV8Isolate);
	v8::HandleScope handle_scope(m_pV8Isolate);

	v8::Local<v8::Function> fnLocal = pFunc->Get(m_pV8Isolate);
	
	if(!fnLocal->IsFunction())
	{
		m_pV8Isolate->ThrowException( v8::String::NewFromUtf8( m_pV8Isolate, "Attempting to set event handler with a value that is not a function.").ToLocalChecked() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Registers a panel for listening to messages
//-----------------------------------------------------------------------------
uint32 CUIEngine::RegisterJSEventHandler( CPanoramaSymbol symEvent, IUIPanel *pPanel, IUIPanel *pContextPanel, 
	v8::Persistent< v8::Function > *pFunc )
{
	VPROF_BUDGET( "CUIEngine::RegisterJSEventHandler", VPROF_BUDGETGROUP_TENFOOT );

	ValidateJSFunction( pFunc );

#if ( V8_CTX_DBG_SPEW_ENABLED )
	V8_CtxDbgMsg( "RegisterJSEventHandler: string %s, panel %x, \n", symEvent.String(), pPanel );
#endif	// V8_CTX_DBG_SPEW_ENABLED

	if( !pPanel )
	{
		Assert( "Failed to find event handlers for panel" );
		return (uint32)-1;
	}

	VecEventHandlers_t *pvec = ((CUIPanel*)pPanel)->GetMutableEventHandlers();
	EventHandler_t handler;
	handler.symEvent = symEvent;
	handler.pjsHandler = pFunc;
	handler.pContextPanel = pContextPanel;
	handler.unHandlerId = ++m_unNextEventHandlerId;

	IncrementEventHandlerCount( symEvent, false );

	pvec->AddToTail( handler );

	return handler.unHandlerId;
}

//-----------------------------------------------------------------------------
// Purpose: Unregister event handler from JS
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterJSEventHandler( CPanoramaSymbol symEvent, IUIPanel *pPanel, uint32 unHandlerId )
{
	VPROF_BUDGET( "CUIEngine::UnregisterJSEventHandler", VPROF_BUDGETGROUP_TENFOOT );

	if( !pPanel )
	{
		Assert( "Failed to find event handlers for panel" );
		return;
	}

	// remove this event handler
	bool bRemoved = false;
	IUIPanel *pContextPanel = NULL;
	VecEventHandlers_t *pvec = ((CUIPanel*)pPanel)->GetMutableEventHandlers();
	FOR_EACH_VEC( *pvec, i )
	{
		EventHandler_t &existingHandler = pvec->Element( i );

		if( existingHandler.symEvent == symEvent && existingHandler.unHandlerId == unHandlerId )
		{
			bRemoved = true;
			pContextPanel = existingHandler.pContextPanel;
			existingHandler.pjsHandler->Reset();
			delete existingHandler.pjsHandler;
			pvec->Remove( i );
			break;
		}
	}

	DbgVerify( bRemoved );
	DbgVerify( pContextPanel ); // Has to be a JS event

	DecrementEventHandlerCount( symEvent, true );
}

//-----------------------------------------------------------------------------
// Purpose: Registers a delegate to listen for a specific unhandled event
//-----------------------------------------------------------------------------
uint32 CUIEngine::RegisterJSForUnhandledEvent(CPanoramaSymbol symMsg, IUIPanel *pContextPanel,
	v8::Persistent< v8::Function > *pFunc)
{
	VPROF_BUDGET("CUIEngine::RegisterForUnhandledEvent", VPROF_BUDGETGROUP_TENFOOT);

	ValidateJSFunction( pFunc );

	EventHandler_t handler;
	handler.symEvent = symMsg;
	handler.pjsHandler = pFunc;
	handler.pContextPanel = pContextPanel;
	handler.unHandlerId = ++m_unNextEventHandlerId;

	// Add to m_mapUnhandledEventHandlers
	{
		int iMap = m_mapUnhandledEventHandlers.Find(symMsg);
		if(iMap == m_mapUnhandledEventHandlers.InvalidIndex())
			iMap = m_mapUnhandledEventHandlers.Insert(symMsg, new VecEventHandlers_t());

		VecEventHandlers_t *pvecHandlers = m_mapUnhandledEventHandlers.Element(iMap);

		pvecHandlers->AddToTail(handler);
	}

	// Add to m_mapPanelToJSUnhandledEventHandlers
	{
		int iMapByPanel = m_mapPanelToJSUnhandledEventHandlers.Find(pContextPanel);
		if(iMapByPanel == m_mapPanelToJSUnhandledEventHandlers.InvalidIndex())
			iMapByPanel = m_mapPanelToJSUnhandledEventHandlers.Insert(pContextPanel, new VecEventHandlers_t());

		VecEventHandlers_t *pvecHandlersByPanel = m_mapPanelToJSUnhandledEventHandlers.Element(iMapByPanel);
		pvecHandlersByPanel->AddToTail(handler);
	}

	IncrementEventHandlerCount(symMsg, true);

	return handler.unHandlerId;
}

//-----------------------------------------------------------------------------
// Purpose: Registers a delegate to listen for a specific unhandled event
//-----------------------------------------------------------------------------
void CUIEngine::RegisterForUnhandledEvent( CPanoramaSymbol symMsg, CUtlAbstractDelegate pFunc )
{
	VPROF_BUDGET( "CUIEngine::RegisterForUnhandledEvent", VPROF_BUDGETGROUP_TENFOOT );

	int iMap = m_mapUnhandledEventHandlers.Find( symMsg );
	if ( iMap == m_mapUnhandledEventHandlers.InvalidIndex() )
		iMap = m_mapUnhandledEventHandlers.Insert( symMsg, new VecEventHandlers_t() );

	VecEventHandlers_t *pvecHandlers = m_mapUnhandledEventHandlers.Element( iMap );

#ifdef _DEBUG
	FOR_EACH_VEC( *pvecHandlers, i )
	{
		EventHandler_t &existingHandler = pvecHandlers->Element( i );
		if ( existingHandler.symEvent == symMsg && existingHandler.pHandler.IsEqual( pFunc ) )
		{
			AssertMsg( false, "Function already registered for this event" );
			return;
		}
	}
#endif

	EventHandler_t handler;
	handler.symEvent = symMsg;
	handler.pHandler = pFunc;
	handler.pjsHandler = NULL;
	pvecHandlers->AddToTail( handler );

	IncrementEventHandlerCount( symMsg, true );

	// Keep track of all the events this pointer is associated with
	void *pEventHandler = pFunc.UnsafeGetThisPtr();
	if ( pEventHandler )
	{
		int iMessageMap = m_mapUnhandledEventHandlerMessages.Find( pEventHandler );
		if ( iMessageMap == m_mapUnhandledEventHandlerMessages.InvalidIndex() )
			iMessageMap = m_mapUnhandledEventHandlerMessages.Insert( pEventHandler, new CUtlVector< CPanoramaSymbol >() );

		CUtlVector< CPanoramaSymbol > *pvecMessages = m_mapUnhandledEventHandlerMessages.Element( iMessageMap );
		pvecMessages->AddToTail( symMsg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Increment the count of handlers for this event, used for early out elsewhere
//-----------------------------------------------------------------------------
void CUIEngine::IncrementEventHandlerCount( const CPanoramaSymbol &symEvent, bool bUnhandledHandler, bool bPanelTypeHandler )
{
	if( bPanelTypeHandler )
		Assert( !bUnhandledHandler );

	int iCount = m_mapEventsToHandlerCounts.Find( symEvent );
	if ( iCount != m_mapEventsToHandlerCounts.InvalidIndex() )
	{
		if ( bUnhandledHandler )
			m_mapEventsToHandlerCounts[iCount].m_nUnhandledHandlers++;
		else if( bPanelTypeHandler )
			m_mapEventsToHandlerCounts[iCount].m_nPanelTypeHandlers++;
		else
			m_mapEventsToHandlerCounts[iCount].m_nPanelHandlers++;
	}
	else
	{
		HandlerCount_t count;
		if( bUnhandledHandler )
		{
			count.m_nUnhandledHandlers = 1;
		}
		else if( bPanelTypeHandler )
		{
			count.m_nPanelTypeHandlers = 1;
		}
		else
		{
			count.m_nPanelHandlers = 1;
		}

		m_mapEventsToHandlerCounts.Insert( symEvent, count );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Decrement the count of handlers for this event, used for early out elsewhere
//-----------------------------------------------------------------------------
void CUIEngine::DecrementEventHandlerCount( const CPanoramaSymbol &symEvent, bool bUnhandledHandler )
{
	int iCount = m_mapEventsToHandlerCounts.Find( symEvent );
	if ( iCount != m_mapEventsToHandlerCounts.InvalidIndex() )
	{
		if ( bUnhandledHandler )
			m_mapEventsToHandlerCounts[iCount].m_nUnhandledHandlers--;
		else
			m_mapEventsToHandlerCounts[iCount].m_nPanelHandlers--;
	}
	else
	{
		AssertMsg( false, "m_mapEventsToHandlerCounts missing expected event" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Unregisters a delegate that was listening for a specific unhandled event
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterForUnhandledEvent( CPanoramaSymbol symMsg, CUtlAbstractDelegate pFunc )
{
	VPROF_BUDGET( "CUIEngine::UnregisterForUnhandledEvent", VPROF_BUDGETGROUP_TENFOOT );

	int iMap = m_mapUnhandledEventHandlers.Find( symMsg );
	if ( iMap == m_mapUnhandledEventHandlers.InvalidIndex() )
	{
		AssertMsg( false, "Unregistering an unhandled event handler that was not previously registered" );
		return;
	}

	// remove this event handler
	bool bRemoved = false;
	VecEventHandlers_t *pvecHandlers = m_mapUnhandledEventHandlers.Element( iMap );
	FOR_EACH_VEC( *pvecHandlers, i )
	{
		EventHandler_t &existingHandler = pvecHandlers->Element( i );
		if ( existingHandler.symEvent == symMsg && existingHandler.pHandler.IsEqual( pFunc ) )
		{
			bRemoved = true;
			pvecHandlers->Remove( i );
			break;
		}
	}

	DbgVerify( bRemoved );
	if ( bRemoved )
	{
		DecrementEventHandlerCount( symMsg, true );
	}

	// clean up map if that was the last handler for this event
	if ( pvecHandlers->Count() == 0 )
	{
		m_mapUnhandledEventHandlers.RemoveAt( iMap );
		delete pvecHandlers;
	}

	// Keep track of all the events this pointer is associated with
	void *pEventHandler = pFunc.UnsafeGetThisPtr();
	if ( pEventHandler )
	{
		int iMessageMap = m_mapUnhandledEventHandlerMessages.Find( pEventHandler );
		if ( iMessageMap != m_mapUnhandledEventHandlerMessages.InvalidIndex() )
		{
			CUtlVector< CPanoramaSymbol > *pvecMessages = m_mapUnhandledEventHandlerMessages.Element( iMessageMap );
			pvecMessages->FindAndFastRemove( symMsg );
			if ( pvecMessages->Count() == 0 )
			{
				m_mapUnhandledEventHandlerMessages.RemoveAt( iMessageMap );
				delete pvecMessages;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Unregisters all unhandled event handlers with a given pointer
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterForUnhandledEvents( void *pEventHandler )
{
	if ( !pEventHandler )
		return;

	int iMessageMap = m_mapUnhandledEventHandlerMessages.Find( pEventHandler );
	if ( iMessageMap == m_mapUnhandledEventHandlerMessages.InvalidIndex() )
		return;

	CUtlVector< CPanoramaSymbol > *pvecMessages = m_mapUnhandledEventHandlerMessages.Element( iMessageMap );
	m_mapUnhandledEventHandlerMessages.RemoveAt( iMessageMap );

	FOR_EACH_VEC( *pvecMessages, i )
	{
		CPanoramaSymbol symMsg = ( *pvecMessages )[ i ];

		int iMap = m_mapUnhandledEventHandlers.Find( symMsg );
		if ( iMap == m_mapUnhandledEventHandlers.InvalidIndex() )
		{
			Assert( false );
			continue;
		}

		VecEventHandlers_t *pvecHandlers = m_mapUnhandledEventHandlers.Element( iMap );
		for ( int iHandler = pvecHandlers->Count() - 1; iHandler >= 0; iHandler-- )
		{
			EventHandler_t &eventHandler = ( *pvecHandlers )[ iHandler ];
			if ( !eventHandler.pHandler )
				continue;

			void *pExistingEventHandler = eventHandler.pHandler.UnsafeGetThisPtr();
			if ( !pExistingEventHandler || pExistingEventHandler != pEventHandler )
				continue;

			pvecHandlers->Remove( iHandler );
			DecrementEventHandlerCount( symMsg, true );
		}

		if ( pvecHandlers->Count() == 0 )
		{
			m_mapUnhandledEventHandlers.RemoveAt( iMap );
			delete pvecHandlers;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Registers a delegate to listen for a specific unhandled event
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterJSForUnhandledEvent( CPanoramaSymbol symMsg, uint32 unHandlerId )
{
	VPROF_BUDGET( "CUIEngine::UnregisterJSForUnhandledEvent", VPROF_BUDGETGROUP_TENFOOT );

	// Remove from m_mapUnhandledEventHandlers
	IUIPanel *pContextPanel = NULL;
	{
		int iMap = m_mapUnhandledEventHandlers.Find( symMsg );
		if ( iMap == m_mapUnhandledEventHandlers.InvalidIndex() )
		{
			AssertMsg( false, "Unregistering an unhandled event handler that was not previously registered" );
			return;
		}

		// remove this event handler
		bool bRemoved = false;
		VecEventHandlers_t *pvecHandlers = m_mapUnhandledEventHandlers.Element( iMap );
		FOR_EACH_VEC( *pvecHandlers, i )
		{
			EventHandler_t &existingHandler = pvecHandlers->Element( i );

			if ( existingHandler.symEvent == symMsg && existingHandler.unHandlerId == unHandlerId )
			{
				bRemoved = true;
				pContextPanel = existingHandler.pContextPanel;
				existingHandler.pjsHandler->Reset();
				delete existingHandler.pjsHandler;
				pvecHandlers->Remove( i );
				break;
			}
		}

		DbgVerify( bRemoved );
		DbgVerify( pContextPanel ); // Has to be a JS event

		// clean up map if that was the last handler for this event
		if ( pvecHandlers->Count() == 0 )
		{
			m_mapUnhandledEventHandlers.RemoveAt( iMap );
			delete pvecHandlers;
		}
	}

	DecrementEventHandlerCount( symMsg, true );

	if ( !pContextPanel )
	{
		// Shouldn't happen. Already have DbgVerify for this case.
		return;
	}

	// Remove from m_mapPanelToJSUnhandledEventHandlers
	{
		int iMapByPanel = m_mapPanelToJSUnhandledEventHandlers.Find( pContextPanel );
		if ( iMapByPanel == m_mapPanelToJSUnhandledEventHandlers.InvalidIndex() )
		{
			AssertMsg( false, "Event handler was in one map but not the other." );
			return;
		}

		bool bRemoved = false;
		VecEventHandlers_t *pvecHandlersByPanel = m_mapPanelToJSUnhandledEventHandlers.Element( iMapByPanel );
		FOR_EACH_VEC( *pvecHandlersByPanel, i )
		{
			EventHandler_t &existingHandler = pvecHandlersByPanel->Element( i );

			if ( existingHandler.symEvent == symMsg && existingHandler.unHandlerId == unHandlerId )
			{
				bRemoved = true;
				pvecHandlersByPanel->Remove( i );
				break;
			}
		}

		DbgVerify( bRemoved );

		// clean up map if that was the last handler for this event
		if ( pvecHandlersByPanel->Count() == 0 )
		{
			m_mapPanelToJSUnhandledEventHandlers.RemoveAt( iMapByPanel );
			delete pvecHandlersByPanel;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
JSGenericCallbackHandle_t CUIEngine::RegisterJSGenericCallback( panorama::IUIPanel *pContextPanel, 
	v8::Handle< v8::Function > callbackFunc )
{
	// create the callback
	JSGenericCallback_t *pCallback = new JSGenericCallback_t( m_nNextGenericCallbackHandle++, pContextPanel, m_pV8Isolate, callbackFunc );

	// Add to m_mapPanelToJSGenericCallbacks
	int iContextPanelCallbackList = m_mapPanelToJSGenericCallbacks.Find( pContextPanel );
	if ( iContextPanelCallbackList == m_mapPanelToJSGenericCallbacks.InvalidIndex() )
	{
		iContextPanelCallbackList = m_mapPanelToJSGenericCallbacks.Insert( pContextPanel, new VecJSGenericCallbackPtr_t() );
	}

	VecJSGenericCallbackPtr_t *pvecHandlersByPanel = m_mapPanelToJSGenericCallbacks[ iContextPanelCallbackList ];
	pvecHandlersByPanel->AddToTail( pCallback );

	m_AllJSGenericCallbacks.Insert( pCallback->m_nCallbackHandle, pCallback );

	return pCallback->m_nCallbackHandle;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUIEngine::InvokeJSGenericCallback( JSGenericCallbackHandle_t nHandle, int nArgs, v8::Handle<v8::Value> *pArgs, v8::Handle< v8::Value > *pOutRetVal )
{
	VPROF_BUDGET( "CUIEngine::InvokeJSGenericCallback", VPROF_BUDGETGROUP_TENFOOT );

	// make sure the callback handle is still valid
	int iCallbackIndex = m_AllJSGenericCallbacks.Find( nHandle );
	if ( iCallbackIndex == m_AllJSGenericCallbacks.InvalidIndex() )
	{
		if ( pOutRetVal )
		{
			*pOutRetVal = v8::Null( m_pV8Isolate );
		}
		return false;
	}

	JSGenericCallback_t *pCallback = m_AllJSGenericCallbacks[ iCallbackIndex ];

	// call the callback
	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::EscapableHandleScope handle_scope( m_pV8Isolate );

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pCallback->m_pContextPanel );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *pContextRef );

	v8::Context::Scope context_scope( context );

	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( m_pV8Isolate, pCallback->m_jsHandler );
	v8::Handle<v8::Object> obj = context->Global();

	v8::Local<v8::Value> returnval = RunJSFunctionInternal( pCallback->m_pContextPanel, context, obj, fnLocal, nArgs, pArgs, false );
	
	if ( pOutRetVal )
	{
		*pOutRetVal = handle_scope.Escape( returnval );
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUIEngine::ClearFileCache()
{
	m_pUILayoutManager->ClearFileCache();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUIEngine::PrintCacheStatus()
{
	m_pUILayoutManager->PrintCacheStatus();
}

#if defined( SOURCE2_PANORAMA ) && !defined (PANORAMA_USE_S1WRAPPER)
static void panorama_PrintCacheStatus( const CCommandContext &ctx, const CCommand &args )
{
	panorama::UIEngine()->PrintCacheStatus();
}

static ConCommand panorama_PrintCacheStatus_Cmd( "@panorama_print_cache_status", panorama_PrintCacheStatus, "Print internal panorama refcounts for every file" );
#endif


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterJSGenericCallback( JSGenericCallbackHandle_t nHandle )
{
	int iCallbackIndex = m_AllJSGenericCallbacks.Find( nHandle );
	if ( iCallbackIndex == m_AllJSGenericCallbacks.InvalidIndex() )
	{
		AssertMsg1( false, "UnregisterJSGenericCallback - Unrecognized generic callback handle: %d\n", nHandle );
		return;
	}

	// remove from the global list
	JSGenericCallback_t *pCallback = m_AllJSGenericCallbacks[ iCallbackIndex ];
	m_AllJSGenericCallbacks.RemoveAt( iCallbackIndex );

	Assert( pCallback->m_nCallbackHandle == nHandle );

	// also remove it from the list for the associated panel
	int iContextPanelCallbackList = m_mapPanelToJSGenericCallbacks.Find( pCallback->m_pContextPanel );
	Assert( iContextPanelCallbackList != m_mapPanelToJSGenericCallbacks.InvalidIndex() );

	if ( iContextPanelCallbackList != m_mapPanelToJSGenericCallbacks.InvalidIndex() )
	{
		VecJSGenericCallbackPtr_t *pList = m_mapPanelToJSGenericCallbacks[ iContextPanelCallbackList ];
		bool bResult = pList->FindAndFastRemove( pCallback );
		AssertMsg( bResult, "pList->FindAndFastRemove( pCallback ) failed" );
		if ( pList->Count() == 0 )
		{
			// removing the final callback for this panel - remove and delete the empty list
			m_mapPanelToJSGenericCallbacks.RemoveAt( iContextPanelCallbackList );
			delete pList;
		}
	}

	// nuke it!
	delete pCallback;
}

//-----------------------------------------------------------------------------
// Purpose: Return count of registered JavaScript scopes (classes, namespaces).
//-----------------------------------------------------------------------------
int CUIEngine::GetNumRegisterJSScopes()
{
	return m_vecRegisterJSScopes.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get information on a registration scope.
//-----------------------------------------------------------------------------
void CUIEngine::GetRegisterJSScopeInfo( int nScope, RegisterJSScopeInfo_t *pInfo )
{
	Assert( nScope >= 0 && nScope < m_vecRegisterJSScopes.Count() );
	RegisterJSScopeInfoInternal_t *pScope = &m_vecRegisterJSScopes[nScope];

	memset( pInfo, 0, sizeof(*pInfo) );
	pInfo->pName = pScope->strName;
	pInfo->pDescription = pScope->strDescription;
	pInfo->nEntries = pScope->vecEntries.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get information on a registration entry.
//-----------------------------------------------------------------------------
void CUIEngine::GetRegisterJSEntryInfo( int nScope, int nEntry, RegisterJSEntryInfo_t *pInfo )
{
	Assert( nScope >= 0 && nScope < m_vecRegisterJSScopes.Count() );
	RegisterJSScopeInfoInternal_t *pScope = &m_vecRegisterJSScopes[nScope];
	Assert( nEntry >= 0 && nEntry < pScope->vecEntries.Count() );
	RegisterJSEntryInfoInternal_t *pEntry = &pScope->vecEntries[nEntry];

	memset( pInfo, 0, sizeof(*pInfo) );
	pInfo->pName = pEntry->strName;
	pInfo->pDescription = pEntry->strDescription;
	pInfo->unFlags = pEntry->unFlags;
	pInfo->eDataType = pEntry->eDataType;
	pInfo->unNumParams = pEntry->unNumParams;
	if ( pEntry->unNumParams > 0 && pEntry->unNumParams != RegisterJSEntryInfo_t::k_unNumParamsUnknown )
	{
		Assert( pEntry->unNumParams <= RegisterJSEntryInfo_t::k_unMaxParams );
		for ( int i = 0; i < pEntry->unNumParams; ++i )
		{
			pInfo->pParamNames[i] = pEntry->pParamInfos[i].strName;
			pInfo->pParamTypes[i] = pEntry->pParamInfos[i].eDataType;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Open a JS registration scope (class, namespace).
//-----------------------------------------------------------------------------
int CUIEngine::StartRegisterJSScope( const char *pName, const char *pDesc /* = NULL */ )
{
	Assert( m_nCurRegisterJSScope == -1 );

	m_nCurRegisterJSScope = m_vecRegisterJSScopes.AddToTail();
	RegisterJSScopeInfoInternal_t *pScope = &m_vecRegisterJSScopes[m_nCurRegisterJSScope];
	pScope->strName = pName;
	if ( pDesc )
	{
		pScope->strDescription = pDesc;
	}
	return m_nCurRegisterJSScope;
}

//-----------------------------------------------------------------------------
// Purpose: Close a JS registration scope.
//-----------------------------------------------------------------------------
void CUIEngine::EndRegisterJSScope()
{
	Assert( m_nCurRegisterJSScope >= 0 );
	m_nCurRegisterJSScope = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Start a new registration entry in the current scope.
// Ignore if there is no scope.
//-----------------------------------------------------------------------------
int CUIEngine::NewRegisterJSEntry( const char *pName, uint32 unFlags, const char *pDesc /* = NULL */, RegisterJSType_t eDataType /* = k_ERegisterJSTypeUnknown */ )
{
	if ( m_nCurRegisterJSScope < 0 )
	{
		return -1;
	}

	RegisterJSScopeInfoInternal_t *pScope = &m_vecRegisterJSScopes[m_nCurRegisterJSScope];
	int nIndex = pScope->vecEntries.AddToTail();
	RegisterJSEntryInfoInternal_t *pEntry = &pScope->vecEntries[nIndex];
	pEntry->strName = pName;
	if ( pDesc )
	{
		pEntry->strDescription = pDesc;
	}
	pEntry->unFlags = unFlags;
	pEntry->eDataType = eDataType;
	pEntry->unNumParams = RegisterJSEntryInfo_t::k_unNumParamsUnknown;
	return nIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Sets parameter information for the current registration entry.
//-----------------------------------------------------------------------------
void CUIEngine::SetRegisterJSEntryParams( int nEntry, uint8 unNumParams, RegisterJSType_t *pParamTypes, const char *pchArgNames )
{
	if ( m_nCurRegisterJSScope < 0 || nEntry < 0 )
	{
		// Silently ignore if we aren't recording registrations.
		return;
	}

	RegisterJSScopeInfoInternal_t *pScope = &m_vecRegisterJSScopes[m_nCurRegisterJSScope];
	RegisterJSEntryInfoInternal_t *pEntry = &pScope->vecEntries[nEntry];

	// Make sure we aren't destroying already-set information.
	Assert( pEntry->unNumParams == RegisterJSEntryInfo_t::k_unNumParamsUnknown );

	Assert( unNumParams <= RegisterJSEntryInfo_t::k_unMaxParams );
	pEntry->unNumParams = unNumParams;
	CUtlStringList vecArgNames;
	if ( pchArgNames )
	{
		vecArgNames.SplitString( pchArgNames, "," );
	}
	for ( int i = 0; i < unNumParams; ++i )
	{
		pEntry->pParamInfos[i].eDataType = pParamTypes[i];

		if ( i < vecArgNames.Count() )
		{
			pEntry->pParamInfos[i].strName = vecArgNames[i];
			pEntry->pParamInfos[i].strName.Trim();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes all registered event handlers on the panel
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterEventHandlersForPanel( IUIPanel *pPanel )
{
	// remove this event handler
	VecEventHandlers_t *pvec = ((CUIPanel*)pPanel)->GetMutableEventHandlers();
	FOR_EACH_VEC_BACK( *pvec, i )
	{
		EventHandler_t handler = pvec->Element( i );
		DecrementEventHandlerCount( handler.symEvent, false );
		if( handler.pjsHandler )
		{
			handler.pjsHandler->Reset();
			delete handler.pjsHandler;
		}
	}
	pvec->Purge();
}


//-----------------------------------------------------------------------------
// Purpose: Registers a panel for listening to messages
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterEventHandler( CPanoramaSymbol symEvent, IUIPanel *pPanel, CUtlAbstractDelegate pFunc )
{
	VPROF_BUDGET( "CUIEngine::UnregisterEventHandler", VPROF_BUDGETGROUP_TENFOOT );

	if ( !pPanel )
	{
		Assert( "Failed to find event handlers for panel" );
		return;
	}

	// remove this event handler
	VecEventHandlers_t *pvec = ((CUIPanel*)pPanel)->GetMutableEventHandlers();
	bool bFound = false;
	FOR_EACH_VEC_BACK( *pvec, i )
	{
		EventHandler_t handler = pvec->Element( i );
		if ( symEvent != handler.symEvent || !pFunc.IsEqual( handler.pHandler ) )
			continue;

		bFound = true;
		pvec->FastRemove( i );
		break;
	}

	Assert( bFound );

	DecrementEventHandlerCount( symEvent, false );
}


//-----------------------------------------------------------------------------
// Purpose: What is the last/current event being dispatched, since we don't pass
// the event itself to handlers this is needed if the same function handles multiple
// events and needs to branch behavior on type
//-----------------------------------------------------------------------------
CPanoramaSymbol CUIEngine::GetLastDispatchedEventSymbol()
{
	return m_symLastDispatchedEvent;
}


//-----------------------------------------------------------------------------
// Purpose: What panel registered for the event being dispatched?
//-----------------------------------------------------------------------------
 IUIPanel *CUIEngine::GetLastDispatchedEventTargetPanel()
{
	return m_pLastDispatchedEventTargetPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Sends a message
//-----------------------------------------------------------------------------
bool CUIEngine::DispatchEvent( IUIEvent *pEvent )
{	
	bool bHandled = false;
	m_symLastDispatchedEvent = pEvent->GetEventType();

#if ( V8_CTX_DBG_SPEW_ENABLED )
	// Uncomment when required, otherwise too much spew
	// V8_CtxDbgMsg( "DispatchEvent: %s\n", pEvent->GetEventType().String() );
#endif	// V8_CTX_DBG_SPEW_ENABLED

	UISoundSystem()->ServiceAudio();

	// Check if the event is filtered and let the filters raise replacement events if needed.
	if ( BIsEventFiltered( pEvent ) )
	{
		delete pEvent;
		return true;
	}

	CFastTimer eventTimer;
	eventTimer.Start();

	HandlerCount_t counts;
	int iHashCounts = m_mapEventsToHandlerCounts.Find( pEvent->GetEventType() );
	if ( iHashCounts != m_mapEventsToHandlerCounts.InvalidIndex() )
		counts = m_mapEventsToHandlerCounts[iHashCounts];

	if( counts.m_nPanelHandlers || counts.m_nPanelTypeHandlers )
	{
		// bubble event up until it is handled
		IUIPanel *pPanel = (IUIPanel*)(pEvent->GetTargetPanel().Get());
		m_pLastDispatchedEventTargetPanel = pPanel;

		{
			VPROF_BUDGET_DETAILED( "CUIEngine::DispatchEvent - Walk Panels", VPROF_BUDGETGROUP_TENFOOT );
			while ( !bHandled && pPanel != NULL )
			{
				if( counts.m_nPanelHandlers )
				{
					const VecEventHandlers_t &vecHandlers = ((CUIPanel*)pPanel)->GetEventHandlers();
					FOR_EACH_VEC_BACK( vecHandlers, i )
					{
						const EventHandler_t handler = vecHandlers.Element( i );
						if( pEvent->GetEventType() != handler.symEvent )
							continue;

						// Should only have one of the types of handlers defined
						Assert( !handler.pjsHandler || handler.pHandler.IsEmpty() );

						if( !handler.pHandler.IsEmpty() )
						{
							VPROF_BUDGET_DETAILED( pEvent->GetEventType().String(), VPROF_BUDGETGROUP_TENFOOT );
							bHandled = pEvent->Dispatch( handler.pHandler );
						}

						if( bHandled )
							break;

						if( handler.pjsHandler )
						{
							VPROF_BUDGET_DETAILED( pEvent->GetEventType().String(), VPROF_BUDGETGROUP_TENFOOT );
							bHandled = DispatchJSEventHandler( pEvent, pPanel, handler );
						}

						if( bHandled )
							break;
					}
				}


				// Check base panel2d registrations for all panels
				if( !bHandled && counts.m_nPanelTypeHandlers )
				{
					VPROF_BUDGET_DETAILED( "CUIEngine::DispatchEvent - ByPanelType", VPROF_BUDGETGROUP_TENFOOT );
					CPanoramaSymbol symInvalid;
					CPanoramaSymbol symPanel = pPanel->ClientPtr()->GetPanelType();
					while( symPanel != symInvalid )
					{
						int iPanelMap = m_mapPanelTypeEventHandlers.Find( symPanel );
						if( iPanelMap != m_mapPanelTypeEventHandlers.InvalidIndex() )
						{
							CUtlHashMap < CPanoramaSymbol, PanelTypeEventHandler_t, CDefEquals< CPanoramaSymbol > > *pEventMap = m_mapPanelTypeEventHandlers[iPanelMap];
							int iMap = pEventMap->Find( pEvent->GetEventType() );
							if( iMap != pEventMap->InvalidIndex() )
							{
								VPROF_BUDGET( pEvent->GetEventType().String(), VPROF_BUDGETGROUP_TENFOOT );
								PanelTypeEventHandler_t &handler = pEventMap->Element( iMap );
								handler.del.UnsafeThisPointerSlam( handler.m_bIsUIPanelThisPtr ? (void*)pPanel : (void*)(pPanel->ClientPtr()) );
								bHandled = pEvent->Dispatch( handler.del );

								if( bHandled )
									break;
							}
						}

						symPanel = GetPanelBaseClassSymbol( symPanel );
					}
				}


				if ( !bHandled )
				{
					pPanel = pPanel->GetParent();
					m_pLastDispatchedEventTargetPanel = pPanel;
				}
			}
		}
	}

	if ( counts.m_nUnhandledHandlers )
	{
		// if message was not handled by a panel, try unhandled event handlers
		{
			VPROF_BUDGET( "CUIEngine::DispatchEvent - Find Unhandled", VPROF_BUDGETGROUP_TENFOOT );
			if ( !bHandled )
			{
				int iMap = m_mapUnhandledEventHandlers.Find( pEvent->GetEventType() );
				if ( iMap != m_mapUnhandledEventHandlers.InvalidIndex() )
				{
					VecEventHandlers_t &vec = *(m_mapUnhandledEventHandlers.Element( iMap ));
					//int cOriginalVec = vec.Count();
					FOR_EACH_VEC_BACK( vec, i )
					{
						EventHandler_t &handler = vec[i];
						if ( !handler.pHandler.IsEmpty() )
						{
							VPROF_BUDGET( pEvent->GetEventType().String(), VPROF_BUDGETGROUP_TENFOOT );
							bHandled |= pEvent->Dispatch( handler.pHandler );
							// Note: unhandled event handlers cannot early-out the remainder
							// of event dispatch loop, but the fact that one of the unhandled
							// handlers handled the event is preserved and returned to caller
							// in case event was dispatched synchronously
						}
						else if ( handler.pjsHandler && handler.pContextPanel )
						{
							VPROF_BUDGET( pEvent->GetEventType().String(), VPROF_BUDGETGROUP_TENFOOT );
							if ( UIEngineInternal()->IsValidPanelPointer( handler.pContextPanel ) )
							{
								DispatchJSEventHandler( pEvent, handler.pContextPanel, handler );
							}
							else
							{
								AssertMsg1( false, "Context panel for JavaScript event handler for %s is invalid. Forgot to unregister?", pEvent->GetEventType().String() );
								UnregisterJSForUnhandledEvent( handler.symEvent, handler.unHandlerId );
							}
						}

						//AssertMsg1( cOriginalVec == vec.Count(), "Event handler for %s removed itself", pEvent->GetEventType().String() );
					}
				}
			}
		}
	}

	eventTimer.End();

	EventStatsUpdate( pEvent->GetEventType(), eventTimer.GetDuration() );

	delete pEvent;
	return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Calls a JS event handler.
//-----------------------------------------------------------------------------
bool CUIEngine::DispatchJSEventHandler( IUIEvent *pEvent, const IUIPanel *pPanel, const EventHandler_t &handler )
{
	VPROF_BUDGET( pEvent->GetEventType().String(), VPROF_BUDGETGROUP_TENFOOT );

	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope( m_pV8Isolate );

	v8::Persistent<v8::Context> *pContextRef = GetContextForPanel( pPanel );
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( m_pV8Isolate, *pContextRef );

	v8::Context::Scope context_scope( context );

	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( m_pV8Isolate, *(handler.pjsHandler) );

	v8::Handle<v8::Object> obj = context->Global();

	int nArgs = 0;
	v8::Handle<v8::Value> *pArguments = NULL;

	pEvent->GetJavaScriptArgs( &nArgs, &pArguments );
		
	v8::Local<v8::Value> return_val = RunJSFunctionInternal( handler.pContextPanel, context, obj, fnLocal, nArgs, pArguments, false );
 
	if ( pArguments )
		delete[] pArguments;

	if ( !return_val.IsEmpty() )
	{
		v8::Local<v8::Boolean> jsReturn( return_val->ToBoolean( GetV8Isolate() ) );
		return jsReturn->BooleanValue( v8::Isolate::GetCurrent() );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Queues a message for delivery async, thread safe for posting, 
// always dispatched on main thread.
//-----------------------------------------------------------------------------
void CUIEngine::DispatchEventAsync( float flDelay, IUIEvent *pEvent )
{
	QueuedEvent_t queued = { GetCurrentFrameTime() + flDelay, pEvent };
	m_tslNewAsyncEvents.PushItem( queued );
}


//-----------------------------------------------------------------------------
// Purpose: Dispatches queued messages that have passed 
//-----------------------------------------------------------------------------
void CUIEngine::DispatchQueuedEvent( CLimitTimer &limit )
{
	VPROF_BUDGET( "CUIEngine::DispatchQueuedEvent", VPROF_BUDGETGROUP_TENFOOT );
	double flNow = GetCurrentFrameTime();	

	// Move events from thread safe list into sorted vector
	QueuedEvent_t queued;
	while( m_tslNewAsyncEvents.Count() )
	{
		if ( m_tslNewAsyncEvents.PopItem( &queued ) )
		{
			m_vecQueuedEvents.InsertAfterEqual( queued );
		}
	}

	if ( s_convarLargeDispatchEventQueue.GetInt() > 0 &&
		 m_vecQueuedEvents.Count() > s_convarLargeDispatchEventQueue.GetInt() )
	{
		CUtlMap< CPanoramaSymbol, int > mapQueuedEvents;
		mapQueuedEvents.SetLessFunc( DefLessFunc( CPanoramaSymbol ) );

		FOR_EACH_VEC( m_vecQueuedEvents, i )
		{
#if 0 // It's usually important to see large numbers of pending events too.
			if ( m_vecQueuedEvents[ i ].flDispatch > flNow )
				continue;
#endif

			int iMap = mapQueuedEvents.Find( m_vecQueuedEvents[ i ].pEvent->GetEventType() );
			if ( iMap == mapQueuedEvents.InvalidIndex() )
				mapQueuedEvents.Insert( m_vecQueuedEvents[ i ].pEvent->GetEventType(), 1 );
			else
				mapQueuedEvents.Element( iMap )++;
		}

		if ( mapQueuedEvents.Count() > 0 )
		{
			CUtlVector< int > vecIndices;
			vecIndices.EnsureCapacity( mapQueuedEvents.Count() );
			FOR_EACH_MAP_FAST( mapQueuedEvents, i )
			{
				vecIndices.AddToTail( i );
			}
			vecIndices.SortPredicate( [ &]( int iLeft, int iRight )
									  {
										  return mapQueuedEvents.Element( iLeft ) > mapQueuedEvents.Element( iRight );
									  } );

			Msg( "DispatchQueuedEvent: %d events queued for %d distinct events\n", m_vecQueuedEvents.Count(), vecIndices.Count() );
			for ( int i : vecIndices )
			{
				Msg( "   %s: %d\n", mapQueuedEvents.Key( i ).String(), mapQueuedEvents.Element( i ) );
			}
		}
	}

	bool bLimitReached = false;
	int nEventsToDispatch = m_vecQueuedEvents.Count();
	// Dispatch any messages which are ready
	int cDispatch = 0;
	FOR_EACH_VEC( m_vecQueuedEvents, i )
	{
		if ( m_vecQueuedEvents[i].flDispatch > flNow || bLimitReached )
			break;

		// dispatch will delete the event
		DispatchEvent( m_vecQueuedEvents[i].pEvent );

		cDispatch++;
		
		if( i % 10 )
		{
			UISoundSystem()->ServiceAudio();
			bLimitReached = limit.BLimitReached();
		}
	}
	m_vecQueuedEvents.RemoveMultiple( 0, cDispatch );

	if ( bLimitReached && m_vecQueuedEvents.Count() > 50 )
	{
		Log_Warning( LOG_PANORAMA, "DispatchAsyncEvent backlog, failed to dispatch all this frame. %d of %d remaining\n", m_vecQueuedEvents.Count(), nEventsToDispatch );
	
		// Dump summary
		if ( panorama_dump_events_backlog.GetBool() )
		{
			// TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "Backlog" );

			Log_Warning( LOG_PANORAMA, " Events backlog summary\n" );

			struct QueuedEventSortedByType_t
			{
				QueuedEvent_t *pQueuedEvent;

				bool operator< ( const QueuedEventSortedByType_t &rhs ) const 
				{
					return pQueuedEvent->pEvent->GetEventType() < rhs.pQueuedEvent->pEvent->GetEventType();
				}
			};
		
			// Print event backlog sorted by event type
			CUtlSortVector< QueuedEventSortedByType_t > vecQueuedEventsDump;

			{
				// TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "Backlog - sort" );
				FOR_EACH_VEC( m_vecQueuedEvents, i )
				{
					QueuedEventSortedByType_t ev;
					ev.pQueuedEvent = &m_vecQueuedEvents[i];
					vecQueuedEventsDump.InsertAfterEqual( ev );
				}
			}

			{
				// TM_ZONE( TELEMETRY_LEVEL0, TMZF_NONE, "Backlog - log" );

				UtlSymId_t prevSymbolID = vecQueuedEventsDump[0].pQueuedEvent->pEvent->GetEventType();
				int nPrevIDStart = 0;
				int nCount = vecQueuedEventsDump.Count();

				for ( int i = 1; i < nCount; i++ )
				{
					IUIEvent *pEvent = vecQueuedEventsDump[i].pQueuedEvent->pEvent;
				
					if ( prevSymbolID != pEvent->GetEventType() )
					{
						Log_Warning( LOG_PANORAMA, " %48s: %d occurences\n", panorama::ResolveSymbol( prevSymbolID ), i - nPrevIDStart );
						prevSymbolID = pEvent->GetEventType();
						nPrevIDStart = i;
					}
				}

				// Print last batch
				Log_Warning( LOG_PANORAMA, " %48s: %d occurences\n", panorama::ResolveSymbol( prevSymbolID ), nCount - nPrevIDStart );
			}
		}	
	}
}

void CUIEngine::DrawEventStats( CUIRenderEngine * pRenderEngine )
{
	EventStatsDraw( pRenderEngine );
}

//-----------------------------------------------------------------------------
// Purpose: Finds the install path
//-----------------------------------------------------------------------------
const char *CUIEngine::GetApplicationInstallPath()
{
#if !defined( SOURCE2_PANORAMA )
	if ( m_strAppInstallPath.IsEmpty() )
	{
		char *pchPath= new char[ MAX_UNICODE_PATH_IN_UTF8 ];
		if ( Plat_GetExecutablePathUTF8( pchPath, MAX_UNICODE_PATH_IN_UTF8 ) )
		{
			V_StripFilename( pchPath );
#if defined(LINUX) || defined(PANORAMA_PUBLIC_STEAM_SDK)
			// pull off the bins sub directory ( /linux32 for example)
			V_StripLastDir( pchPath, MAX_UNICODE_PATH_IN_UTF8 );
#endif
			m_strAppInstallPath = pchPath;
		}

		delete [] pchPath;
		Assert( !m_strAppInstallPath.IsEmpty() );
	}

	return m_strAppInstallPath.String();
#else
	return "";
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Finds the userdata path
//-----------------------------------------------------------------------------
const char *CUIEngine::GetApplicationUserDataPath()
{
	if ( m_strAppUserDataPath.IsEmpty() )
	{

#if defined(WIN32) || defined( SOURCE2_PANORAMA_FIXME )
		m_strAppUserDataPath = GetApplicationInstallPath();
#elif defined(POSIX)
		// XXX TODO make this better: can we get the actual universe from a config setting?
		// This may be called during startup before we even try to connect. Ick. henryg
		char buf[PATH_MAX];
		m_strAppUserDataPath = Steam_GetBaseUserFolder( k_EUniverseInvalid, buf, sizeof(buf) );
#else
#error GetApplicationUserDataPath not implemented
#endif
	}

	return m_strAppUserDataPath.String();
}


//-----------------------------------------------------------------------------
// Purpose: Registers a path to watch for changed files
//-----------------------------------------------------------------------------
void CUIEngine::AddDirectoryChangeWatch( const char *pchPath )
{
	// Don't add an identical path twice!
	FOR_EACH_VEC( m_vecDirWatchers, i )
	{
		if ( m_vecDirWatchers[i]->m_sFullPath == pchPath )
			return;
	}

	DirWatchers_t *pWatcher = new DirWatchers_t();
	pWatcher->m_dirWatcher.SetDirToWatch( pchPath );
	pWatcher->m_sFullPath = pchPath;

	m_vecDirWatchers.AddToTail( pWatcher );
}


//-----------------------------------------------------------------------------
// Purpose: Registers a named path that hangs off of the executable
//-----------------------------------------------------------------------------
void CUIEngine::RegisterNamedLocalPath( const char *pathName, const char *pchLocalPath, bool bWatchForFileChanges, bool bAddToOverwriteIfExists  )
{
	Assert( pathName );
	Assert( pchLocalPath );

	uint32 unLen = V_strlen( pathName );
	if ( pathName[0] != '{' || pathName[unLen-1] != '}' )
	{
		AssertMsg( false, "Path names must be enclosed in {} braces." );
		return;
	}

	CUtlString strPath = GetApplicationInstallPath();
	strPath += pchLocalPath;
	V_FixSlashes( strPath.Access() );
	V_FixDoubleSlashes( strPath.Access() );
	V_RemoveDotSlashes( strPath.Access() );

	if ( bWatchForFileChanges )
	{
		AddDirectoryChangeWatch( strPath.Access() );
	}

	short iDict = m_dictNamedPaths.Find( pathName );
	bool bExists = iDict != m_dictNamedPaths.InvalidIndex();
	
	if ( bExists && bAddToOverwriteIfExists )
	{
		int iMap = m_mapNamedOverwritePaths.Find( pathName );
		if ( iMap == m_mapNamedOverwritePaths.InvalidIndex() )
		{
			iMap = m_mapNamedOverwritePaths.Insert( pathName, new CUtlVector< CUtlString >() );
		}
		m_mapNamedOverwritePaths[ iMap ]->AddToHead( strPath );
	}
	else
	{
		m_dictNamedPaths.Insert( pathName, strPath );
	}

}


//-----------------------------------------------------------------------------
// Purpose: Registers a named path that hangs off of the userdata folder
//-----------------------------------------------------------------------------
void CUIEngine::RegisterNamedUserPath( const char *pathName, const char *pchUserPath, bool bWatchForFileChanges, bool bAddToOverwriteIfExists )
{
	Assert( pathName );
	Assert( pchUserPath );

	uint32 unLen = V_strlen( pathName );
	if ( pathName[0] != '{' || pathName[unLen-1] != '}' )
	{
		AssertMsg( false, "Path names must be enclosed in {} braces." );
		return;
	}

	CUtlString strPath = GetApplicationUserDataPath();
	strPath += pchUserPath;
	V_FixSlashes( strPath.Access() );
	V_FixDoubleSlashes( strPath.Access() );
	V_RemoveDotSlashes( strPath.Access() );

	if ( bWatchForFileChanges )
	{
		AddDirectoryChangeWatch( strPath.Access() );
	}

	short iDict = m_dictNamedPaths.Find( pathName );
	bool bExists = iDict != m_dictNamedPaths.InvalidIndex();

	if ( bExists && bAddToOverwriteIfExists )
	{
		int iMap = m_mapNamedOverwritePaths.Find( pathName );
		if ( iMap == m_mapNamedOverwritePaths.InvalidIndex() )
		{
			iMap = m_mapNamedOverwritePaths.Insert( pathName, new CUtlVector< CUtlString >() );
		}
		m_mapNamedOverwritePaths[ iMap ]->AddToHead( strPath );
	}
	else
	{
		m_dictNamedPaths.Insert( pathName, strPath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Registers path for custom fonts to be loaded from
//-----------------------------------------------------------------------------
void CUIEngine::RegisterCustomFontPath( const char *pchFontPath )
{
	Assert( pchFontPath );

#ifdef PANORAMA_USE_S1WRAPPER
	const char *pchContainerDir = nullptr;
	char szFontFullPath[ MAX_PATH ];
	g_pFullFileSystem->RelativePathToFullPath( pchFontPath, "MOD", szFontFullPath, sizeof( szFontFullPath ) );
	pchFontPath = szFontFullPath;
#elif defined( SOURCE2_PANORAMA )
	const char *pchContainerDir = g_pApplication->GetModPath();
#else
	const char *pchContainerDir = GetApplicationInstallPath();
#endif

	UITextServices()->BLoadCustomFontCollection( pchContainerDir, pchFontPath );
}


//-----------------------------------------------------------------------------
// Purpose: Registers an X-Header to be sent on web requests
//-----------------------------------------------------------------------------
void CUIEngine::RegisterXHeader( const char *pchHeaderName, const char *pchHeaderValue )
{
	// If the header is already in the list, overwrite the previous value
	FOR_EACH_VEC( m_vecXHeaders, i )
	{
		if ( m_vecXHeaders[i].strName == pchHeaderName )
		{
			m_vecXHeaders[i].strValue = pchHeaderValue;
			return;
		}
	}

	XHeader_t header;
	header.strName = pchHeaderName;
	header.strValue = pchHeaderValue;

	m_vecXHeaders.AddToTail( header );
}


//-----------------------------------------------------------------------------
// Purpose: Registers the count of registered X-Headers
//-----------------------------------------------------------------------------
int CUIEngine::GetXHeaderCount() const
{
	return m_vecXHeaders.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the name and value for the specified X-Header
//-----------------------------------------------------------------------------
void CUIEngine::GetXHeader( int i, CUtlString &strName, CUtlString &strValue ) const
{
	strName = m_vecXHeaders[i].strName;
	strValue = m_vecXHeaders[i].strValue;
}


//-----------------------------------------------------------------------------
// Purpose: Adds common HTTP headers to a request
//-----------------------------------------------------------------------------
void CUIEngine::AddCommonHeadersToHttpRequest( HTTPRequestHandle hRequest ) const
{
	// set os
	const char *pchUserAgent = "Unknown;tenfoot";
	if ( IsWindows() )
		pchUserAgent = "Windows;tenfoot";
	else if ( IsOSX() )
		pchUserAgent = "Macintosh;tenfoot";
	else if ( IsLinux() )
		pchUserAgent = "Linux;tenfoot";

#if !defined( SOURCE2_PANORAMA )
	if ( g_WebBrowserOS.Exists() )
	{
		const char *pchOS = CommandLine()->ParmValue( g_WebBrowserOS.GetHParam(), "" );
		if ( V_strcmp( pchOS, "windows" ) == 0 )
			pchUserAgent = "Windows;tenfoot";
		else if ( V_strcmp( pchOS, "osx" ) == 0 )
			pchUserAgent = "Macintosh;tenfoot";
		else if ( V_strcmp( pchOS, "linux" ) == 0 )
			pchUserAgent = "Linux;tenfoot";
	}
#endif

	ClientHTTP()->SetHTTPRequestUserAgentInfo( hRequest, pchUserAgent );
	ClientHTTP()->SetHTTPRequestHeaderValue( hRequest, "X-ValveUserAgent", CFmtStr1024( "panorama %s %s", __DATE__, __TIME__ ).String() );

#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	ClientHTTP()->SetHTTPRequestHeaderValue( hRequest, "X-SteamClientVersion", CFmtStr1024( "%llu", ClientUtils()->GetBuildID() ).String() );
#endif

	for ( int i = 0; i < UIEngine()->GetXHeaderCount(); i++ )
	{
		CUtlString strName;
		CUtlString strValue;
		UIEngine()->GetXHeader( i, strName, strValue );
		ClientHTTP()->SetHTTPRequestHeaderValue( hRequest, strName, strValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Registers a named path that hangs off of the userdata folder
//-----------------------------------------------------------------------------
void CUIEngine::RegisterNamedRemoteHost( const char *hostName, const char *pchRemoteHost )
{
	Assert( hostName );
	Assert( pchRemoteHost );

	NamedHost_t host;
	host.m_strHost = pchRemoteHost;

	m_mapNamedHosts.Insert( hostName, host );
}


//-----------------------------------------------------------------------------
// Purpose: Resolves a path that may contain named path portions, etc, to a full local path
//-----------------------------------------------------------------------------
CUtlString CUIEngine::ResolvePath( const char *pchPath )
{
	CFileResource file( pchPath );
	return file.GetReferencePath();
}


//-----------------------------------------------------------------------------
// Purpose: Static member for returning a reference to an empty cookie list when needed
//-----------------------------------------------------------------------------
CUtlVector< CUtlString > CUIEngine::sm_vecEmptyCookieList;


//-----------------------------------------------------------------------------
// Purpose: Set cookie header for named remote host, this will get used on future requests to that host
// from the image loader subsystem
//-----------------------------------------------------------------------------
void CUIEngine::SetCookieHeaderForNamedRemoteHost( const char *hostName, const CUtlVector<CUtlString> &vecCookies )
{
	int iMap = m_mapNamedHosts.Find( hostName );
	if ( iMap == m_mapNamedHosts.InvalidIndex() )
		return;

	m_mapNamedHosts[iMap].m_vecCookieHeaders.RemoveAll();
	m_mapNamedHosts[iMap].m_vecCookieHeaders.AddVectorToTail( vecCookies );
}


//-----------------------------------------------------------------------------
// Purpose: Set cookie header for remote host, this will get used on future requests to that host
// from the image loader subsystem
//-----------------------------------------------------------------------------
void CUIEngine::SetCookieHeaderForRemoteHost( const char *hostName, const CUtlVector<CUtlString> &vecCookies )
{
	int iMap = m_mapHostCookies.Find( hostName );
	if ( iMap == m_mapHostCookies.InvalidIndex() )
		iMap = m_mapHostCookies.Insert( hostName );
	
	m_mapHostCookies[iMap].RemoveAll();
	m_mapHostCookies[iMap].AddVectorToTail( vecCookies );
}


//-----------------------------------------------------------------------------
// Purpose: Finds cookie header value for remote host
//-----------------------------------------------------------------------------
const CUtlVector<CUtlString> &CUIEngine::GetCookieHeadersForRemoteHost( const char *hostName )
{
	int iMap = m_mapHostCookies.Find( hostName );
	if ( iMap != m_mapHostCookies.InvalidIndex() )
		return m_mapHostCookies[iMap];
	
	return sm_vecEmptyCookieList;
}


//-----------------------------------------------------------------------------
// Purpose: Finds cookie value for remote host
//-----------------------------------------------------------------------------
bool CUIEngine::GetCookieValueForRemoteHost( const char *hostName, const char *cookieName, CUtlString *pstrCookieValue )
{
	int iMap = m_mapHostCookies.Find( hostName );
	if ( iMap == m_mapHostCookies.InvalidIndex() )
		return false;

	CCopyableUtlVector<CUtlString> &vecCookies = m_mapHostCookies[iMap];

	CUtlString strSearch;
	strSearch.Format( "%s=", cookieName );
	uint32 cubSearch = strSearch.Length();

	// Look for a cookie that starts with "cookieName=" and use everything
	// after up until the null terminator or a semicolon as the value.
	FOR_EACH_VEC( vecCookies, iVecCookie )
	{
		CUtlString strCookie = vecCookies[iVecCookie];
		uint32 cubCookie = strCookie.Length();

		if ( V_strncmp( strCookie.String(), strSearch.String(), cubSearch ) == 0 )
		{
			// Truncate strCookie after the semicolon
			for ( uint32 i = cubSearch; i < cubCookie && strCookie.String()[i] != '\0'; i++ )
			{
				if ( strCookie.Access()[i] == ';' )
				{
					strCookie.Access()[i] = '\0';
					break;
				}
			}

			// TODO: This should be unescaped
			pstrCookieValue->Set( &strCookie.String()[cubSearch] );
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a named host
//-----------------------------------------------------------------------------
const char *CUIEngine::GetRemoteHostForNamedHost( const char *hostName )
{
	int iMap = m_mapNamedHosts.Find( hostName );
	if ( iMap == m_mapNamedHosts.InvalidIndex() )
		return NULL;

	return m_mapNamedHosts.Element( iMap ).m_strHost.String();
}


//-----------------------------------------------------------------------------
// Purpose: Finds a named path
//-----------------------------------------------------------------------------
const char *CUIEngine::GetLocalPathForNamedPath( const char *pathName )
{
	short iDict = m_dictNamedPaths.Find( pathName );
	if ( iDict == m_dictNamedPaths.InvalidIndex() )
		return NULL;

	return m_dictNamedPaths.Element( iDict ).String();
}


//-----------------------------------------------------------------------------
// Purpose: Finds a named path and create full path from relative pathname based on existence
//-----------------------------------------------------------------------------
void CUIEngine::GetLocalPathForRelativePath( const char *pchLocalPathName, const char *pchRelativePathname, CUtlString &strLocalPath )
{
	char pchFullPath[ MAX_UNICODE_PATH ];
	bool bSuccess = false;

	int iMap = m_mapNamedOverwritePaths.Find( pchLocalPathName );
	if ( iMap != m_mapNamedOverwritePaths.InvalidIndex() )
	{
		CUtlVector< CUtlString > *vecNamed = m_mapNamedOverwritePaths[ iMap ];
		for ( int i=0; i<vecNamed->Count(); i++ )
		{
#if defined( SOURCE2_PANORAMA )
			V_snprintf( pchFullPath, MAX_UNICODE_PATH, "%s/%s", vecNamed->Element(i).String(), pchRelativePathname );
			V_FixSlashes( pchFullPath );
#else
			V_MakeAbsolutePath( pchFullPath, MAX_UNICODE_PATH, pchRelativePathname, vecNamed->Element(i).String() );
#endif
			V_RemoveDotSlashes( pchFullPath );

			bSuccess = UIEngine()->UIFileSystem()->FileExists( pchFullPath );
			if ( bSuccess )
				break;
		}
	}

	if ( !bSuccess )
	{
		short iDict = m_dictNamedPaths.Find( pchLocalPathName );
		if ( iDict == m_dictNamedPaths.InvalidIndex() )
		{
			pchFullPath[0] = '\0'; // empty string indicates failure
		}
		else
		{
#if defined( SOURCE2_PANORAMA )
			V_snprintf( pchFullPath, MAX_UNICODE_PATH, "%s/%s", m_dictNamedPaths.Element( iDict ).String(), pchRelativePathname );
			V_FixSlashes( pchFullPath );
#else
			V_MakeAbsolutePath( pchFullPath, MAX_UNICODE_PATH, pchRelativePathname,  m_dictNamedPaths.Element( iDict ).String() );
#endif
			V_RemoveDotSlashes( pchFullPath );
		}
	}

	strLocalPath.Set( pchFullPath );
}


//-----------------------------------------------------------------------------
// Purpose: Finds cookie header value for named host
//-----------------------------------------------------------------------------
const CUtlVector<CUtlString> &CUIEngine::GetCookieHeadersForNamedRemoteHost( const char *hostName )
{
	int iMap = m_mapNamedHosts.Find( hostName );
	if ( iMap == m_mapNamedHosts.InvalidIndex() )
		return sm_vecEmptyCookieList;

	return m_mapNamedHosts.Element( iMap ).m_vecCookieHeaders;
}


//-----------------------------------------------------------------------------
// Purpose: Gets/creates cookie container for a domain
//-----------------------------------------------------------------------------
HTTPCookieContainerHandle CUIEngine::GetCookieContainerForDomain( const char *pchHost )
{
	int iMap = m_mapDomainCookieContainers.Find( pchHost );
	if ( iMap == m_mapDomainCookieContainers.InvalidIndex() )
		iMap = m_mapDomainCookieContainers.Insert( pchHost, ClientHTTP()->CreateCookieContainer( true ) );

	return m_mapDomainCookieContainers.Element( iMap );
}


//-----------------------------------------------------------------------------
// Purpose: Sets a cookie
//-----------------------------------------------------------------------------
bool CUIEngine::BSetCookieForWebRequests( const char *pchHost, const char *pchPath, const char *pchCookie )
{
	if ( V_isempty( pchHost ) || V_isempty( pchPath ) || V_isempty( pchCookie ) )
		return false;

	HTTPCookieContainerHandle hContainer = GetCookieContainerForDomain( pchHost );
	return ClientHTTP()->SetCookie( hContainer, pchHost, pchPath, pchCookie );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to clear a cookie
//-----------------------------------------------------------------------------
bool CUIEngine::BClearCookieForWebRequests( const char *pchHost, const char *pchPath, const char *pchCookie )
{
	if ( V_isempty( pchHost ) || V_isempty( pchPath ) || V_isempty( pchCookie ) )
		return false;

	CUtlString strCookie;
	strCookie.Format( "%s=0;expires=Thu, 01-Jan-1970 00:00:01 GMT;", pchCookie );

	HTTPCookieContainerHandle hContainer = GetCookieContainerForDomain( pchHost );
	return ClientHTTP()->SetCookie( hContainer, pchHost, pchPath, strCookie );
}


//-----------------------------------------------------------------------------
// Purpose: Register a new event filter
//-----------------------------------------------------------------------------
void CUIEngine::RegisterEventFilter( CUtlAbstractDelegate pFunc )
{
	VPROF_BUDGET( "CUIEngine::RegisterEventFilter", VPROF_BUDGETGROUP_TENFOOT );

	Assert( !m_vecEventFilters.HasElement( pFunc ) );
	m_vecEventFilters.AddToTail( pFunc );
}


//-----------------------------------------------------------------------------
// Purpose: Unregister an event filter
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterEventFilter( CUtlAbstractDelegate pFunc )
{
	VPROF_BUDGET( "CUIEngine::UnregisterEventFilter", VPROF_BUDGETGROUP_TENFOOT );

	Assert( m_vecEventFilters.HasElement( pFunc ) );
	m_vecEventFilters.FindAndRemove( pFunc );
}


//-----------------------------------------------------------------------------
// Purpose: Check if an event is filtered, returning true if it is.
//-----------------------------------------------------------------------------
bool CUIEngine::BIsEventFiltered( IUIEvent *pEvent )
{
	bool bBlocked = false;
	FOR_EACH_VEC( m_vecEventFilters, i )
	{
		CUtlDelegate< bool ( IUIEvent * ) > del;
		del.SetAbstractDelegate( m_vecEventFilters[i] );
		if ( del( pEvent ) )
		{
			//
			// Even if we're blocked, keep going - other filters
			// may want to take action on the event even if it's
			// already being blocked.
			//
			bBlocked = true;
		}
	}
	return bBlocked;
}


//-----------------------------------------------------------------------------
// Purpose: checks for changed resource files, and passes them onto our subsystems
//-----------------------------------------------------------------------------
bool CUIEngine::AutoReloadChangedFiles()
{
	CUtlString strFile;
	FOR_EACH_VEC( m_vecDirWatchers, i )
	{
		while ( m_vecDirWatchers[i]->m_dirWatcher.GetChangedFile( &strFile ) )
		{
			CUtlString sFullPath = m_vecDirWatchers[i]->m_sFullPath;
			sFullPath += strFile; // make it into a full path
			m_pLocalization->ReloadChangedFile( sFullPath );
			FOR_EACH_VEC( m_vecWindows, iWindow )
			{
				m_vecWindows[iWindow]->ReloadChangedFile( sFullPath );
			}
			m_pUILayoutManager->ReloadChangedFile( sFullPath );
			m_pInputEngine->ReloadChangedFile( sFullPath );
			ReloadChangedFile( sFullPath );
		}
	}

	::DispatchEventAsync( 0.2f, ReloadChangedUIFiles(), (IUIPanel*)NULL );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: return the interface pointer for the input engine
//-----------------------------------------------------------------------------
IUIInput *CUIEngine::UIInputEngine()
{
	return m_pInputEngine;
}


//-----------------------------------------------------------------------------
// Purpose: return the interface pointer for the localize engine
//-----------------------------------------------------------------------------
IUILocalization *CUIEngine::UILocalize()
{
	return m_pLocalization;
}


//-----------------------------------------------------------------------------
// Purpose: return the interface pointer for the sound system
//-----------------------------------------------------------------------------
IUISoundSystem *CUIEngine::UISoundSystem()
{
	return m_pSoundSystem;
}


//-----------------------------------------------------------------------------
// Purpose: Returns language to use for display
//-----------------------------------------------------------------------------
ELanguage CUIEngine::GetDisplayLanguage()
{
#if !defined( SOURCE2_PANORAMA )
	return UILocalize()->CurrentLanguage();
#else
	return GetCurrentInputLocale();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: run a frame of the input engine (and therefore game controller)
//-----------------------------------------------------------------------------
void CUIEngine::RunControllerFrame()
{
	m_pInputEngine->RunFrame();
}


//-----------------------------------------------------------------------------
// Purpose: Add mapping of panel to parent for mouse can activate if parent focused
//-----------------------------------------------------------------------------
void CUIEngine::RegisterMouseCanActivateParent( IUIPanel *pPanel, const char *pchParent )
{
	Assert( m_mapMouseCanActivateIfParent.Find( pPanel ) == m_mapMouseCanActivateIfParent.InvalidIndex() );
	m_mapMouseCanActivateIfParent.InsertOrReplace( pPanel, pchParent );
}


//-----------------------------------------------------------------------------
// Purpose: Remove mapping of panel to parent for mouse can activate if parent focused
//-----------------------------------------------------------------------------
void CUIEngine::UnregisterMouseCanActivateParent( IUIPanel *pPanel )
{
	m_mapMouseCanActivateIfParent.Remove( pPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves registered string for mouse can activate if parent focused for specified panel
//-----------------------------------------------------------------------------
const char *CUIEngine::GetMouseCanActivateParent( IUIPanel *pPanel )
{
	int iMap = m_mapMouseCanActivateIfParent.Find( pPanel );
	if ( !pPanel )
		return NULL;

	return m_mapMouseCanActivateIfParent[ iMap ];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUIEngine::ClearClipboard()
{
	CopyToClipboard( "", "" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUIEngine::CopyToClipboard( const char *pchTextUTF8, const char *pchClipboardPasteStringLocToken )
{
	m_unClipboardHash = HashString( pchTextUTF8 );
	m_sClipboardPasteStringLocToken = pchClipboardPasteStringLocToken;

	CopyToClipboardImpl( pchTextUTF8 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUIEngine::GetClipboardText( CUtlString &strUTF8, CUtlString *out_psPasteStringLocToken ) const
{
	GetClipboardTextImpl( strUTF8 );

	// Make sure the string in our clipboard matches the string that we put there when we assigned
	// our source. This is prevent messing with the clipboard outside of Panorama but having it still
	// think that the text is a URL, a CD key, etc.
	if ( out_psPasteStringLocToken )
	{
#if defined( SOURCE2_PANORAMA )
		const uint32 unClipboardHash = HashString( strUTF8.Get() );
#else
		const uint32 unClipboardHash = HashString( strUTF8.Get(), strUTF8.Length() );
#endif
		const bool bHasValidClipboardSource = m_sClipboardPasteStringLocToken.Length() > 0
										   && unClipboardHash == m_unClipboardHash;

		*out_psPasteStringLocToken = bHasValidClipboardSource
								   ? m_sClipboardPasteStringLocToken
								   : "#UI_Paste_UnknownSource";
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check if overlay instance exists for app/pid pair
//-----------------------------------------------------------------------------
bool CUIEngine::BHasOverlayForApp( uint64 gameID, uint64 ulPID )
{
	OverlayInstance_t instance;
	instance.gameID = gameID;
	instance.ulPID = ulPID;

	return (m_mapOverlayInstances.Find( instance ) != m_mapOverlayInstances.InvalidIndex());
}


//-----------------------------------------------------------------------------
// Purpose: Track overlay instance
//-----------------------------------------------------------------------------
void CUIEngine::TrackOverlayForApp( uint64 gameID, uint64 ulPID, void *pOverlay )
{
	OverlayInstance_t instance;
	instance.gameID = gameID;
	instance.ulPID = ulPID;

	m_mapOverlayInstances.Insert( instance, pOverlay );
}


//-----------------------------------------------------------------------------
// Purpose: Delete overlay instance
//-----------------------------------------------------------------------------
void CUIEngine::DeleteOverlayInstanceForApp( uint64 gameID, uint64 ulPID, void *pOverlay )
{
	OverlayInstance_t instance;
	instance.gameID = gameID;
	instance.ulPID = ulPID;

	int iMap = m_mapOverlayInstances.Find( instance );
	if ( iMap != m_mapOverlayInstances.InvalidIndex() )
	{
		if ( m_mapOverlayInstances[iMap] == pOverlay )
			m_mapOverlayInstances.RemoveAt( iMap );
	}
}


//-----------------------------------------------------------------------------
// Purpose: find the first pid registered for this app
//-----------------------------------------------------------------------------
void *CUIEngine::OverlayForApp( uint64 gameID, uint64 ulPID )
{
	OverlayInstance_t instance;
	instance.gameID = gameID;
	instance.ulPID = ulPID;

	int iMap = m_mapOverlayInstances.Find( instance );
	if ( iMap != m_mapOverlayInstances.InvalidIndex() )
	{
		return m_mapOverlayInstances[iMap];
	}

	return NULL;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Get ready to validate
//-----------------------------------------------------------------------------
bool CUIEngine::PrepareForValidate()
{
	bool bResult = true;
	if( m_pHTMLController )
		bResult &= m_pHTMLController->ChromePrepareForValidate();

	FOR_EACH_VEC( m_vecWindows, i )
	{
		bResult &= m_vecWindows[i]->PrepareForValidate();
	}
#if !defined( PANORAMA_DISABLE_VIDEO )
	VideoPauseForValidation();
#endif

	return bResult;
}


//-----------------------------------------------------------------------------
// Purpose: done validating, continue
//-----------------------------------------------------------------------------
bool CUIEngine::ResumeFromValidate()
{
	bool bResult = true;
	if( m_pHTMLController )
		bResult &= m_pHTMLController->ChromeResumeFromValidate();

	FOR_EACH_VEC( m_vecWindows, i )
	{
		bResult &= m_vecWindows[i]->ResumeFromValidate();
	}
#if !defined( PANORAMA_DISABLE_VIDEO )
	VideoResumeFromValidation();
#endif

	return bResult;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: control/client access to layout manager
//-----------------------------------------------------------------------------
IUILayoutManager *CUIEngine::UILayoutManager()
{
	return m_pUILayoutManager;
}


//-----------------------------------------------------------------------------
// Purpose: Allow access to style factory interface
//-----------------------------------------------------------------------------
IUIStyleFactory *CUIEngine::UIStyleFactory()
{
	return m_pStyleFactory;
}


//-----------------------------------------------------------------------------
// Purpose: Create JSON web api job, use the helpers in uiwebapiclient.h directly 
// instead, this is there for them to use internally
//-----------------------------------------------------------------------------
uint32 CUIEngine::InitiateAsyncJSONWebAPIRequest( EHTTPMethod eMethod, CUtlString strURL, IUIPanel *pCallbackTargetPanel, void *pContext, CJSONWebAPIParams *pParams,  HTTPCookieContainerHandle hCookieContainer )
{
	return InitiateAsyncJSONWebAPIRequestInternal( eMethod, strURL, pCallbackTargetPanel, JSONWebAPIDelegate_t(), pContext, pParams, hCookieContainer );
}


//-----------------------------------------------------------------------------
// Purpose: Create JSON web api job, use the helpers in uiwebapiclient.h directly 
// instead, this is there for them to use internally
//-----------------------------------------------------------------------------
uint32 CUIEngine::InitiateAsyncJSONWebAPIRequest( EHTTPMethod eMethod, CUtlString strURL, JSONWebAPIDelegate_t callback, void *pContext, CJSONWebAPIParams *pParams, HTTPCookieContainerHandle hCookieContainer )
{
	return InitiateAsyncJSONWebAPIRequestInternal( eMethod, strURL, NULL, callback, pContext, pParams, hCookieContainer );
}


//-----------------------------------------------------------------------------
// Purpose: Create JSON web api job, use the helpers in uiwebapiclient.h directly 
// instead, this is there for them to use internally
//-----------------------------------------------------------------------------
uint32 CUIEngine::InitiateAsyncJSONWebAPIRequestInternal( EHTTPMethod eMethod, CUtlString strURL, IUIPanel *pCallbackTargetPanel, JSONWebAPIDelegate_t callback, void *pContext, CJSONWebAPIParams *pParams, HTTPCookieContainerHandle hCookieContainer )
{
	HTTPRequestHandle hRequest = ClientHTTP()->CreateHTTPRequest( eMethod, strURL );
	UIEngineInternal()->AddCommonHeadersToHttpRequest( hRequest );

	// for https requests, we always want to verify the server cert
	// bugbug - only supported on windows/osx because linux can't use OS to verify cert
	if ( StringHasPrefix( strURL, "https" ) && !panorama::g_cvarDeveloper.GetBool() )
		ClientHTTP()->SetHTTPRequestRequiresVerifiedCertificate( hRequest, true );
	
	if ( hCookieContainer != INVALID_HTTPCOOKIE_HANDLE )
		ClientHTTP()->SetHTTPRequestCookieContainer( hRequest, hCookieContainer );

	if ( pParams )
	{
		CUtlVector< CJSONWebAPIParams::WebAPIParam_t > *pParamVec = pParams->AccessParams();
		FOR_EACH_VEC( *pParamVec, i )
		{
			ClientHTTP()->SetHTTPRequestGetOrPostParameter( hRequest, pParamVec->Element( i ).strParamName.String(), pParamVec->Element( i ).strParamValue.String() );
		}
	}

	JSONWebAPIRequestInFlight_t req;
	req.m_strURL = strURL;
	req.m_bCanceled = false;
	req.m_pContext = pContext;
	req.m_callback = callback;
	req.m_TargetPanel = pCallbackTargetPanel;

	m_MapInFlightJSONHTTPRequests.Insert( hRequest, req );

	SteamAPICall_t hSteamAPICall;
	if ( ClientHTTP()->SendHTTPRequest( hRequest, &hSteamAPICall ) )
	{
		// Add call handle to get callback
		m_HTTPRequestCompleted.AddCall( hSteamAPICall );
	}
	else
	{
		// Nothing to do here, shouldn't hit this anyway, internal API error.
		Assert( false );

		// Post failure callback faked up now so we dispatch failure to our callers, this will also release
		HTTPRequestCompleted_t fakeCompletion;
		fakeCompletion.m_hRequest = hRequest;
		fakeCompletion.m_bRequestSuccessful = false;
		fakeCompletion.m_eStatusCode = k_EHTTPStatusCode500InternalServerError;
		fakeCompletion.m_ulContextValue = 0;
		OnHTTPJSONWebAPIRequestFinished( &fakeCompletion, false );
	}

	return (uint32)hRequest;
}


//-----------------------------------------------------------------------------
// Purpose: JSON WebAPI request has finished, look up who we should callback and do so now
//-----------------------------------------------------------------------------
void CUIEngine::OnHTTPJSONWebAPIRequestFinished( HTTPRequestCompleted_t *pParam, bool bIOFailure )
{
	bool bSuccess = false;
	bool bDispatched = false;
	int iReqMap = m_MapInFlightJSONHTTPRequests.InvalidIndex();
	JSONWebAPIRequestInFlight_t *pReq = NULL; 

	if ( pParam )
	{
		if ( pParam->m_bRequestSuccessful )
			bSuccess = true;

		iReqMap = m_MapInFlightJSONHTTPRequests.Find( pParam->m_hRequest );
	}

	if ( iReqMap != m_MapInFlightJSONHTTPRequests.InvalidIndex() )
	{
		pReq = &(m_MapInFlightJSONHTTPRequests[iReqMap]);
	}
	else
	{
		// Should not happen
		Assert( iReqMap != m_MapInFlightJSONHTTPRequests.InvalidIndex() );
	}

	if ( bSuccess && pReq )
	{
		uint32 unBodySize = 0;
		ClientHTTP()->GetHTTPResponseBodySize( pParam->m_hRequest, &unBodySize );
		Msg( "HTTP Response for %s: %d, %d\n", pReq->m_strURL.String(), pParam->m_eStatusCode, unBodySize );
		if ( !pReq->m_bCanceled && pParam->m_eStatusCode == k_EHTTPStatusCode200OK )
		{
			CUtlBuffer buf;
			buf.EnsureCapacity( unBodySize );
			if ( ClientHTTP()->GetHTTPResponseBodyData( pParam->m_hRequest, (uint8*)(buf.Base()), unBodySize ) )
			{
				buf.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
#if defined( SOURCE2_PANORAMA )
				KeyValues *pKeyValues = KeyValues::FromJSON( buf );
#else
				KeyValues *pKeyValues = CreateKeyValuesFromJSON( buf );
#endif
				if ( pKeyValues )
				{
					if ( !pReq->m_callback.IsEmpty() )
						pReq->m_callback( pParam->m_hRequest, pKeyValues, pReq->m_pContext );

					if ( pReq->m_TargetPanel.Get() )
						::DispatchEvent( JSONWebAPIResponse(), pReq->m_TargetPanel.Get(), pKeyValues, pReq->m_pContext );

					bDispatched = true;

					pKeyValues->deleteThis();
				}
			}
		}
		else
		{
			Msg( "HTTP Response failed %s\n", pReq->m_strURL.String() );
		}
	}
	else if ( pReq )
	{
		Msg( "HTTP Response time out %s\n", pReq->m_strURL.String() );
	}

	if ( !bDispatched && pReq )
	{
		if ( !pReq->m_bCanceled )
		{
			if ( !pReq->m_callback.IsEmpty() )
				pReq->m_callback( pParam->m_hRequest, NULL, pReq->m_pContext );

			if ( pReq->m_TargetPanel.Get() )
				::DispatchEvent( JSONWebAPIResponse(), pReq->m_TargetPanel.Get(), (KeyValues*)NULL, pReq->m_pContext );

			bDispatched = true;
		}
	}

	if ( iReqMap != m_MapInFlightJSONHTTPRequests.InvalidIndex() )
		m_MapInFlightJSONHTTPRequests.RemoveAt( iReqMap );

	if ( pParam )
	{
		ClientHTTP()->ReleaseHTTPRequest( pParam->m_hRequest );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Interface to allow animation/render threads to queue a decrement of 
// a ref count on an object next frame in the main thread
//-----------------------------------------------------------------------------
void CUIEngine::QueueDecrementRefNextFrame(CRefCount *pRefCountObj)
{
	m_tslQueuedDecRef.PushItem( pRefCountObj );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to ensure JS request is ok to/from specified domain
//-----------------------------------------------------------------------------
bool CUIEngine::BMatchDomainForJSRequest( panorama::IUIPanel *pContextPanel, const char *pchURL )
{
	// first check to see if this layout file was loaded from a URL
	const char *pchLoaded = pContextPanel->GetLayoutFilePathForJSCheck();
	if ( !pchLoaded )
		return false;

	// try http
	if ( V_strncmp( pchLoaded, "http://", 7 ) == 0 || V_strncmp( pchLoaded, "https://", 8 ) == 0 )
	{
		char rgchTargetDomain[256];
		V_ExtractDomainFromURL( pchURL, rgchTargetDomain, V_ARRAYSIZE( rgchTargetDomain ) );

		char rgchContextDomain[1024];
		V_ExtractDomainFromURL( pchLoaded, rgchContextDomain, V_ARRAYSIZE( rgchContextDomain ) );

		if ( V_stricmp( rgchTargetDomain, rgchContextDomain ) == 0 )
			return true;
	}
	else if ( V_strncmp( pchLoaded, "code://", 7 ) != 0 )
	{

#if defined( SOURCE2_PANORAMA )
		// Source 2, in order to facilitate custom games, currently supports
		// AsyncWebRequest for any URL when the request is coming from the disk
		return true;
#else
		// Treat as loaded from disk. Ok to hit Valve domains
		if ( V_URLContainsDomain( pchURL, "steampowered.com" ) || V_URLContainsDomain( pchURL, "steamcommunity.com" ) || V_URLContainsDomain( pchURL, "valvesoftware.com" ) )
			return true;
#endif
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if domain contexts for panels match
//-----------------------------------------------------------------------------
bool CUIEngine::BMatchingDomainsForJSRequest( IUIPanel *pLHS, IUIPanel *pRHS )
{
	const char *pchLoadedLHS = pLHS->GetLayoutFilePathForJSCheck();
	const char *pchLoadedRHS = pRHS->GetLayoutFilePathForJSCheck();
	if ( !pchLoadedLHS || !pchLoadedRHS )
		return false;

	// if both code, ok
	if ( V_strncmp( pchLoadedLHS, "code://", 7 ) == 0 && V_strncmp( pchLoadedRHS, "code://", 7 ) == 0 )
		return true;

	// is at least left http?
	bool bHTTP = (V_strncmp( pchLoadedLHS, "http://", 7 ) == 0);
	bool bHTTPS = (V_strncmp( pchLoadedLHS, "https://", 8 ) == 0);
	if ( !bHTTP && !bHTTPS )
		return false;

	if ( bHTTP && V_strncmp( pchLoadedRHS, "http://", 7 ) != 0 )
		return false;

	if ( bHTTPS && V_strncmp( pchLoadedRHS, "https://", 8 ) != 0 )
		return false;

	// compare domains
	char rgchDomainLHS[256];
	V_ExtractDomainFromURL( pchLoadedLHS, rgchDomainLHS, V_ARRAYSIZE( rgchDomainLHS ) );

	char rgchDomainRHS[1024];
	V_ExtractDomainFromURL( pchLoadedRHS, rgchDomainRHS, V_ARRAYSIZE( rgchDomainRHS ) );

	return (V_stricmp( rgchDomainLHS, rgchDomainRHS ) == 0);
}


//-----------------------------------------------------------------------------
// Purpose: return the ISteamHTMLSurface interface for the controls lib
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
ISteamHTMLSurface *CUIEngine::AccessHTMLController()
{ 
	return steamAPIContext.SteamHTMLSurface();  
}
#endif

//-----------------------------------------------------------------------------
// Purpose: return the ISteamHTMLSurface interface for the controls lib
//-----------------------------------------------------------------------------
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
ISteamHTMLSurface *CUIEngine::AccessHTMLController()
{
	return SteamHTMLSurface();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Create a new top level OpenVR overlay window
//-----------------------------------------------------------------------------
#if !defined( SOURCE2_PANORAMA )
IUIWindow *CUIEngine::CreateNewOpenVROverlayWindow( uint32 width, uint32 height, vr::VROverlayHandle_t ulOverlayHandle, bool bKeepInputFocusOnGamepadFocusLost, bool bIgnoreGamepadFocus )
{
	vrapi::EnsureOpenVRAPILoaded();

	CTopLevelWindowOpenVROverlay * pWindowImpl = new CTopLevelWindowOpenVROverlay( this );
	if ( !pWindowImpl->BInitializeSurface( width, height, ulOverlayHandle, bKeepInputFocusOnGamepadFocusLost, bIgnoreGamepadFocus ) )
	{
		delete pWindowImpl;
		return NULL;
	}

	CTopLevelWindow *pWindow = pWindowImpl;
	if ( !pWindow->FinishInitialization() )
	{
		delete pWindow;
		return NULL;
	}

	m_vecWindows.AddToTail( pWindow );
	return pWindow;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Create or reuse a memory stack for render commands
//-----------------------------------------------------------------------------
CMemoryStack *CUIEngine::AcquireRenderCommandMemoryStack()
{
	CMemoryStack *pMemoryStack = nullptr;

	if ( m_listAvailableCommandMemoryStacks.PopItem( &pMemoryStack ) )
	{
		pMemoryStack->FreeAll( false );
	}
	else
	{
		pMemoryStack = new CMemoryStack();
		pMemoryStack->Init( "panorama RenderCommand_t memory stack", k_unRenderCommandStackSize );
	}

	return pMemoryStack;
}


//-----------------------------------------------------------------------------
// Purpose: Return a memory stack of render commands for reuse
//-----------------------------------------------------------------------------
void CUIEngine::ReleaseMemoryCommandStack( CMemoryStack *pMemoryStack )
{
	m_listAvailableCommandMemoryStacks.PushItem( pMemoryStack );
}


//-----------------------------------------------------------------------------
// Purpose: Dumps the list of panorama event types and their documentation
//-----------------------------------------------------------------------------
CON_COMMAND_F( dump_panorama_events, "print panorama event types and their documentation", 0 )
{
	CUIEngine *pEngine = UIEngineInternal();
	if ( !pEngine )
		return;

	EUniverse eUniverse = steamAPIContext.SteamUtils() ? steamAPIContext.SteamUtils()->GetConnectedUniverse() : k_EUniversePublic;

	bool bInternal = eUniverse != k_EUniversePublic;

	CUIEngine::EEventDocumentationType eDocType = CUIEngine::k_eEventDocumentationType_External;
	if ( bInternal && args.ArgC() > 1 )
	{
		if ( V_stricmp( args[ 1 ], "all" ) == 0 )
		{
			eDocType = CUIEngine::k_eEventDocumentationType_All;
		}
		else if ( V_stricmp( args[ 1 ], "internal" ) == 0 )
		{
			eDocType = CUIEngine::k_eEventDocumentationType_Internal;
		}
		else if ( V_stricmp( args[ 1 ], "external" ) == 0 )
		{
			eDocType = CUIEngine::k_eEventDocumentationType_External;
		}
	}

	UIEngineInternal()->DumpEventDocumentation( eDocType );
}

//-----------------------------------------------------------------------------
// Purpose: Print documentation about registered event types
//-----------------------------------------------------------------------------
void CUIEngine::DumpEventDocumentation( EEventDocumentationType eDocumentationType )
{
	CUtlVector< int > vecSortedIndices;
	vecSortedIndices.EnsureCapacity( m_mapEventRegistrations.Count() );
	FOR_EACH_MAP( m_mapEventRegistrations, iMap )
	{
		const UIEventFactory &factory = m_mapEventRegistrations.Element( iMap );

		if ( eDocumentationType != CUIEngine::k_eEventDocumentationType_All && ( !factory.m_pchDocumentationDescription || factory.m_pchDocumentationDescription[ 0 ] == '\0' ) )
			continue;

		if ( eDocumentationType == CUIEngine::k_eEventDocumentationType_External && ( factory.m_eDocFlags & k_eEventDocFlagInternalOnly ) )
			continue;

		vecSortedIndices.AddToTail( iMap );
	}

	vecSortedIndices.SortPredicate( [ this ]( int iLeft, int iRight ) {
		const CPanoramaSymbol &symLeft = m_mapEventRegistrations.Key( iLeft );
		const CPanoramaSymbol &symRight = m_mapEventRegistrations.Key( iRight );
		return V_stricmp( symLeft.String(), symRight.String() ) < 0;
	} );

	// Output the event list in wiki table format

	Msg( "{| class=\"wikitable\"\n" );
	Msg( "! Event\n" );
	Msg( "! Panel Event\n" );
	Msg( "! Description\n" );

	FOR_EACH_VEC( vecSortedIndices, i )
	{
		int iMap = vecSortedIndices[ i ];
		const CPanoramaSymbol &symEvent = m_mapEventRegistrations.Key( iMap );
		const UIEventFactory &factory = m_mapEventRegistrations.Element( iMap );

		Msg( "|-\n" );
		CUtlString strArgs = factory.m_pfnFormatUIEventArgs( factory.m_pchDocumentationArgs );
		Msg( "| <code>%s(%s)</code>\n", symEvent.String(), strArgs.Get() );
		Msg( "| %s\n", factory.m_bPanelEvent ? "Yes" : "No" );
		Msg( "| %s\n", factory.m_pchDocumentationDescription );
	}

	Msg( "|}\n" );
}

//-----------------------------------------------------------------------------
static const char* g_RegisterJSTypeToString[] = 
{
	"unknown",				// k_ERegisterJSTypeUnknown,
	"invalid",				// k_ERegisterJSTypeInvalid,
	"void",					// k_ERegisterJSTypeVoid,
	"bool",					// k_ERegisterJSTypeBool,
	"int8",					// k_ERegisterJSTypeInt8,
	"uint8",				// k_ERegisterJSTypeUint8,
	"int16",				// k_ERegisterJSTypeInt16,
	"uint16",				// k_ERegisterJSTypeUint16,
	"int32",				// k_ERegisterJSTypeInt32,
	"uint32",				// k_ERegisterJSTypeUint32,
	"int64",				// k_ERegisterJSTypeInt64,
	"uint64",				// k_ERegisterJSTypeUint64,
	"float",				// k_ERegisterJSTypeFloat,
	"double",				// k_ERegisterJSTypeDouble,
	"cstring",				// k_ERegisterJSTypeConstString,
	"cstring",				// k_ERegisterJSTypePanoramaSymbol,
	"js_raw_arg",			// k_ERegisterJSTypeRawV8Args,
	"sf_compat_accessor",	// k_ERegisterJSTypeScaleformCompatAccessor,
	"sf_compat_args",		// k_ERegisterJSTypeScaleformCompatArgs,
	"cstring",				// k_ERegisterJSTypeUtlString,
};

COMPILE_TIME_ASSERT( V_ARRAYSIZE( g_RegisterJSTypeToString ) == k_ERegisterJSTypeMax );

static const char *ConvertRegisterJSTypeToString( RegisterJSType_t type )
{
	if ( ( type < 0 ) || ( type >= ARRAYSIZE( g_RegisterJSTypeToString ) ) )
		return "";

	return g_RegisterJSTypeToString[type];
}

//-----------------------------------------------------------------------------
// Purpose: Output the list of given entry infos for the given scope as property
// Output in the wiki table format
//-----------------------------------------------------------------------------
static void OuputRegisterJSEntryInfosAsProperty( int nScope, const CUtlVector< int > &vecEntryIndices )
{	
	// table header
	Msg( "{| class=\"standard-table\" style=\"width: 100%;\"\n" );
	Msg( "! Property Name\n" );
	Msg( "! Type\n" );
	Msg( "! ReadOnly\n" );
	Msg( "! Description\n" );

	for ( int i = 0; i < vecEntryIndices.Count(); ++i )
	{
		RegisterJSEntryInfo_t entryInfo;
		UIEngine()->GetRegisterJSEntryInfo( nScope, vecEntryIndices[i], &entryInfo );

		Msg( "|-\n" );
		Msg( "| %s\n", entryInfo.pName );
		Msg( "| %s\n", ConvertRegisterJSTypeToString( entryInfo.eDataType ) );
		Msg( "| %s\n", ( entryInfo.GetEntryType() == RegisterJSEntryInfo_t::k_EAccessorReadOnly ) ? "X" : " " );
		Msg( "| %s\n", entryInfo.pDescription );
	}

	Msg( "|}\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Output the list of given entry infos for the given scope as method
// Output in the wiki table format
//-----------------------------------------------------------------------------
static void OuputRegisterJSEntryInfosAsMethod( int nScope, const CUtlVector< int > &vecEntryIndices )
{
	RegisterJSScopeInfo_t scopeInfo;
	UIEngine()->GetRegisterJSScopeInfo( nScope, &scopeInfo );

	// table header
	Msg( "{| class=\"standard-table\" style=\"width: 100%;\"\n" );
	Msg( "! Method Name\n" );
	Msg( "! Signature\n" );
	Msg( "! Description\n" );

	for ( int i = 0; i < vecEntryIndices.Count(); ++i )
	{
		RegisterJSEntryInfo_t entryInfo;
		UIEngine()->GetRegisterJSEntryInfo( nScope, vecEntryIndices[i], &entryInfo );

		// Generate function signature
		static CFmtStr1024 s_fmtSignature;
		s_fmtSignature.Clear();
		s_fmtSignature.AppendFormat( "%s %s.%s", ConvertRegisterJSTypeToString( entryInfo.eDataType ), scopeInfo.pName, entryInfo.pName );
		s_fmtSignature.Append( '(' );
		if ( entryInfo.unNumParams == RegisterJSEntryInfo_t::k_unNumParamsUnknown )
		{
			// Don't know what the args are, assume raw V8 args.
			s_fmtSignature.Append( ' ' );
			s_fmtSignature.Append( ConvertRegisterJSTypeToString( k_ERegisterJSTypeRawV8Args ) );
			s_fmtSignature.Append( ' ' );
		}
		else if ( entryInfo.unNumParams > 0 )
		{
			s_fmtSignature.Append( ' ' );
			for ( int nArg = 0; nArg < entryInfo.unNumParams; ++nArg )
			{
				if ( nArg > 0 )
				{
					s_fmtSignature.Append( ", " );
				}
				s_fmtSignature.AppendFormat( "%s %s", 
					ConvertRegisterJSTypeToString( entryInfo.pParamTypes[nArg] ),
					entryInfo.pParamNames[nArg] );
			}
			s_fmtSignature.Append( ' ' );
		}
		s_fmtSignature.Append( ')' );

		Msg( "|-\n" );
		Msg( "| %s\n", entryInfo.pName );
		Msg( "| %s\n", s_fmtSignature.String() );
		Msg( "| %s\n", entryInfo.pDescription );
	}

	Msg( "|}\n" );
}


//-----------------------------------------------------------------------------
// Purpose: Dumps the list of panorama js scopes, such as classes, and their 
// associated properties / methods.
// Output in the wiki format
// Note that we can only dump the js scopes currently registered with panorama
// js scopes are added dynamically and the number of scopes can change through
// the lifetime of the game.
//-----------------------------------------------------------------------------
CON_COMMAND_F( dump_panorama_js_scopes, "print panorama js scopes, such as classes, and their associated methods. (wiki table format)", 0 )
{
	// Sort js scopes by name
	CUtlVector< int > vecSortedScopesIndices;
	for ( int nScope = 0; nScope < UIEngine()->GetNumRegisterJSScopes(); ++nScope )
	{
		vecSortedScopesIndices.AddToTail( nScope );
	}
	vecSortedScopesIndices.SortPredicate( []( int iLeft, int iRight ) {
		RegisterJSScopeInfo_t scopeInfoLeft;
		RegisterJSScopeInfo_t scopeInfoRight;
		UIEngine()->GetRegisterJSScopeInfo( iLeft, &scopeInfoLeft );
		UIEngine()->GetRegisterJSScopeInfo( iRight, &scopeInfoRight );
		return V_stricmp( scopeInfoLeft.pName, scopeInfoRight.pName ) < 0;
	} );
	
	for ( int i = 0; i < vecSortedScopesIndices.Count(); ++i)
	{
		int nScope = vecSortedScopesIndices[i];
		RegisterJSScopeInfo_t scopeInfo;
		UIEngine()->GetRegisterJSScopeInfo( nScope, &scopeInfo );

		Msg( "== %s ==\n", scopeInfo.pName );
		if ( scopeInfo.pDescription && *scopeInfo.pDescription )
		{
			Msg( "%s\n", scopeInfo.pDescription );
		}

		if ( scopeInfo.nEntries > 0 )
		{
			// Iterate over entries and build a list of properties 
			// and a list of methods registered on the javascript object
			CUtlVector< int > vecSortedPropertiesIndices;
			CUtlVector< int > vecSortedMethodsIndices;
			for ( int nEntry = 0; nEntry < scopeInfo.nEntries; ++nEntry )
			{
				RegisterJSEntryInfo_t entryInfo;
				UIEngine()->GetRegisterJSEntryInfo( nScope, nEntry, &entryInfo );

				switch ( entryInfo.GetEntryType() )
				{
				case RegisterJSEntryInfo_t::k_EMethod:
				case RegisterJSEntryInfo_t::k_EGlobalFunction:
					vecSortedMethodsIndices.AddToTail( nEntry );
					break;
				case RegisterJSEntryInfo_t::k_EAccessor:
				case RegisterJSEntryInfo_t::k_EAccessorReadOnly :
					vecSortedPropertiesIndices.AddToTail( nEntry );
					break;
				}
			}

			// Sort properties and methods by name
			vecSortedPropertiesIndices.SortPredicate( [nScope]( int iLeft, int iRight ) {
				RegisterJSEntryInfo_t entryInfoLeft;
				RegisterJSEntryInfo_t entryInfoRight;
				UIEngine()->GetRegisterJSEntryInfo( nScope, iLeft, &entryInfoLeft );
				UIEngine()->GetRegisterJSEntryInfo( nScope, iRight, &entryInfoRight );
				return V_stricmp( entryInfoLeft.pName, entryInfoRight.pName ) < 0;
			} );
			vecSortedMethodsIndices.SortPredicate( [nScope]( int iLeft, int iRight ) {
				RegisterJSEntryInfo_t entryInfoLeft;
				RegisterJSEntryInfo_t entryInfoRight;
				UIEngine()->GetRegisterJSEntryInfo( nScope, iLeft, &entryInfoLeft );
				UIEngine()->GetRegisterJSEntryInfo( nScope, iRight, &entryInfoRight );
				return V_stricmp( entryInfoLeft.pName, entryInfoRight.pName ) < 0;
			} );

			// Output properties

			if ( vecSortedPropertiesIndices.Count() > 0 )
			{
				OuputRegisterJSEntryInfosAsProperty( nScope, vecSortedPropertiesIndices );
			}

			// Output methods

			if ( vecSortedMethodsIndices.Count() )
			{
				OuputRegisterJSEntryInfosAsMethod( nScope, vecSortedMethodsIndices );
			}
		}
	}
}

#if V8_DEBUGGING_ENABLED

void OnPanoramaRemoteDebug( IConVar *var, const char *pOldValue, float flOldValue )
{
	CUIEngine* pUIEngine = UIEngineInternal();
	if ( !pUIEngine )
		return;

	bool bNewEnable = panorama_remote_debug.GetFloat() != 0.0f;
	bool bOldEnable = flOldValue != 0.0f;

	if ( bNewEnable && !bOldEnable )
	{
		pUIEngine->EnableRemoteDebugger();
	}
	else if ( !bNewEnable && bOldEnable )
	{
		pUIEngine->DisableRemoteDebugger();
	}
}

void DebuggerRunFrame( void )
{
	UIEngineInternal()->DebuggerRunFrame();
}

void CUIEngine::CaptureJSStackTrace( bool bPrint /*= false*/ )
{
	v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace( m_pV8Isolate, 16 );
	
	int nFrames = stackTrace->GetFrameCount();
	int n;
	for ( n = 0; ( n < nFrames ) && ( n < JS_STACK_FRAME_COUNT ) ; n++ )
	{
		v8::Local< v8::StackFrame > frame = stackTrace->GetFrame( n );
		
		v8::String::Utf8Value script(v8::Isolate::GetCurrent(),  frame->GetScriptName() );
		v8::String::Utf8Value fn(v8::Isolate::GetCurrent(),  frame->GetFunctionName() );
		
		V_sprintf_safe( g_JSStackTrace[n], "%s: %d, %d (%s)\n", *script, frame->GetLineNumber(), frame->GetColumn(), (*fn)[0]? *fn: "..." );
	}
	for ( ; n<JS_STACK_FRAME_COUNT; n++ )
	{
		g_JSStackTrace[n][0] = 0;
	}

	if ( bPrint )
	{
		Msg( "JS stack trace begin.\n" );
		for ( n = 0; g_JSStackTrace[n][0] != 0; n++ )
		{
			Msg( g_JSStackTrace[n] );
		}
		Msg("JS stack trace end.");
	}
}

void CUIEngine::EnableRemoteDebugger()
{
	m_pWebsocketServer = new CWebsocketServer();
	m_pInspectorClient = new InspectorClient( m_pV8Isolate, m_pWebsocketServer );
	if ( !m_pWebsocketServer->Start( V8_DEBUGGER_AGENT_PORT, m_pInspectorClient ) )
	{
		DisableRemoteDebugger();
		panorama_remote_debug.SetValue( 0 );
	}

	v8::Isolate::Scope isolate_scope( m_pV8Isolate );
	v8::HandleScope handle_scope(m_pV8Isolate );

	v8::Local< v8::Context > globalContext = m_V8UIEngineGlobalContext.Get( m_pV8Isolate );
	if ( *globalContext )
	{
		m_pInspectorClient->ContextCreated( globalContext );
	}

	FOR_EACH_MAP_FAST( m_MapPanelV8Contexts, i )
	{
		m_pInspectorClient->ContextCreated( m_MapPanelV8Contexts[i]->Get( m_pV8Isolate ) );
	}
}

void CUIEngine::DisableRemoteDebugger()
{
	if ( m_pWebsocketServer )
	{
		m_pWebsocketServer->Stop();
		delete m_pWebsocketServer;
		m_pWebsocketServer = nullptr;
	}

	if ( m_pInspectorClient )
	{
		delete m_pInspectorClient;
		m_pInspectorClient = nullptr;
	}
}


void CUIEngine::DebuggerFrontEndConnected()
{
}

void CUIEngine::DebuggerFrontEndDisconnected()
{
	m_bDebugFrontEndDisconnected = true;
}

void CUIEngine::DebuggerRunFrame()
{
	if ( panorama_remote_debug.GetBool() )
	{
		// Tick server to receive on Websocket
		if ( m_pWebsocketServer )
		{
			m_pWebsocketServer->RunFrame();
		}

		// If the debugger front end just disconnected, shut down relevant objects
		// and, if debugging enabled, restart them
		if ( m_bDebugFrontEndDisconnected )
		{
			DisableRemoteDebugger();
			if ( panorama_remote_debug.GetBool() )
			{
				EnableRemoteDebugger();
			}
			m_bDebugFrontEndDisconnected = false;
		}
	}
}

bool CUIEngine::IsWebSocketServerConnected()
{ 
	return ( m_pWebsocketServer && m_pWebsocketServer->IsConnected() ); 
}

#endif	// V8_DEBUGGING_ENABLED



#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIEngine::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	VideoValidate( validator, pchName );
#endif
	CUIPanel::ValidateStatics( validator, pchName );

	ValidateObj( m_mapPanels );
	ValidatePtr( m_pInputEngine );
	ValidatePtr( m_pUILayoutManager );
	ValidateObj( m_PanelStylePool );
	ValidateObj( m_treePanelsWaitingAsyncDelete );
	ValidateObj( m_vecFrameFuncs );
	ValidateObj( m_vecDirWatchers );
	FOR_EACH_VEC( m_vecDirWatchers, i )
	{
		DirWatchers_t *pWatcher = m_vecDirWatchers[i];
		validator.ClaimMemory( pWatcher );
		ValidateObj( pWatcher->m_sFullPath );
		ValidateObj( pWatcher->m_dirWatcher );
	}
	ValidateObj( m_QueueScheduledDelegates );
	FOR_EACH_VEC( m_QueueScheduledDelegates, i )
	{
		((ScheduledItem_t *)&m_QueueScheduledDelegates.Element( i ))->Validate( validator, "m_QueueScheduledDelegates" );
	}

	ValidateObj( m_mapOverlayInstances );

	ValidateObj( m_dictNamedPaths );
	FOR_EACH_DICT_FAST( m_dictNamedPaths, i )
	{
		ValidateObj( m_dictNamedPaths.Element( i ) );
	}

	ValidateObj( m_vecXHeaders );
	FOR_EACH_VEC( m_vecXHeaders, i )
	{
		XHeader_t &header = m_vecXHeaders[i];
		ValidateObj( header.strName );
		ValidateObj( header.strValue );
	}

	ValidateObj( m_mapNamedOverwritePaths );
	FOR_EACH_MAP_FAST( m_mapNamedOverwritePaths, i )
	{
		ValidatePtr( m_mapNamedOverwritePaths[i] );
	}

	ValidateObj( m_vecWindows );
	FOR_EACH_VEC( m_vecWindows, i )
	{
		ValidatePtr( m_vecWindows[i] );
	}

	ValidateObj( m_mapPanelTypeEventHandlers );
	FOR_EACH_HASHMAP( m_mapPanelTypeEventHandlers, i )
	{
		ValidatePtr( m_mapPanelTypeEventHandlers[i] );
	}

	ValidateObj( m_mapEventsToHandlerCounts );

	ValidateObj( m_mapUnhandledEventHandlers );
	FOR_EACH_MAP_FAST( m_mapUnhandledEventHandlers, i )
	{
		ValidatePtr( m_mapUnhandledEventHandlers[i] );
		if ( m_mapUnhandledEventHandlers[i] )
		{
			FOR_EACH_VEC( *m_mapUnhandledEventHandlers[i], j )
			{
				validator.ClaimMemory( (*m_mapUnhandledEventHandlers[i])[j].pjsHandler );
			}
		}
	}

	ValidateObj( m_mapPanelToJSUnhandledEventHandlers );
	FOR_EACH_MAP_FAST( m_mapPanelToJSUnhandledEventHandlers, i )
	{
		ValidatePtr( m_mapPanelToJSUnhandledEventHandlers[i] );
	}

	ValidateObj( m_mapPanelToJSGenericCallbacks );
	FOR_EACH_MAP_FAST( m_mapPanelToJSGenericCallbacks, i )
	{
		ValidatePtr( m_mapPanelToJSGenericCallbacks[i] );
	}

	ValidateObj( m_AllJSGenericCallbacks );
	FOR_EACH_MAP_FAST( m_AllJSGenericCallbacks, i )
	{
		validator.ClaimMemory( m_AllJSGenericCallbacks[i] );
	}

	ValidateObj( m_vecRegisterJSScopes );
	FOR_EACH_VEC( m_vecRegisterJSScopes, i )
	{
		ValidateObj( m_vecRegisterJSScopes[i] );
	}
	
	ValidateObj( m_vecEventFilters );

	ValidateObj( m_vecQueuedEvents );
	FOR_EACH_VEC( m_vecQueuedEvents, i )
	{
		ValidatePtr( m_vecQueuedEvents[i].pEvent );
	}
	ValidateObj( m_ConsoleHistory );
	FOR_EACH_VEC( m_ConsoleHistory, i )
	{
		ValidateObj( m_ConsoleHistory[i] );
	}

	ValidateObj( m_mapMouseCanActivateIfParent );
	FOR_EACH_MAP_FAST( m_mapMouseCanActivateIfParent, i )
	{
		ValidateObj( m_mapMouseCanActivateIfParent[i] );
	}

	ValidateObj( m_mapNamedHosts );
	FOR_EACH_MAP_FAST( m_mapNamedHosts, i )
	{
		ValidateObj( m_mapNamedHosts.Key( i ) );
		ValidateObj( m_mapNamedHosts[i].m_vecCookieHeaders );
		FOR_EACH_VEC( m_mapNamedHosts[i].m_vecCookieHeaders, iVec )
			ValidateObj( m_mapNamedHosts[i].m_vecCookieHeaders[iVec] );
		ValidateObj( m_mapNamedHosts[i].m_strHost );
	}

	ValidateObj( m_mapHostCookies );
	FOR_EACH_MAP_FAST( m_mapHostCookies, i )
	{
		ValidateObj( m_mapHostCookies.Key( i ) );
		ValidateObj( m_mapHostCookies[i] );
		FOR_EACH_VEC( m_mapHostCookies[i], iVec )
			ValidateObj( m_mapHostCookies[i][iVec] );
	}

	ValidateObj( m_mapDomainCookieContainers );
	FOR_EACH_MAP_FAST( m_mapDomainCookieContainers, i )
	{
		ValidateObj( m_mapDomainCookieContainers.Key( i ) );
	}

	ValidateObj( m_treeCallBeforeStyleAndLayout );
	ValidatePtr( m_pLocalization );
	ValidateObj( m_strAppInstallPath );
	if( m_pHTMLController )
		m_pHTMLController->Validate( validator,  "ChromeValidate" );

	// all threads should be stopped while we validate.. so ok to walk the TS list

	CUtlVector<QueuedEvent_t> vecTemp;
	m_tslNewAsyncEvents.ValidateDataStructureOnly( validator, "m_tslNewAsyncEvents" );

	QueuedEvent_t e;
	while( m_tslNewAsyncEvents.Count() )
	{
		if( m_tslNewAsyncEvents.PopItem( &e ) )
		{
			ValidatePtr( e.pEvent );
			vecTemp.AddToTail( e );
		}
	}
	FOR_EACH_VEC_BACK( vecTemp, i )
	{
		m_tslNewAsyncEvents.PushItem( vecTemp[i] );
	}

	ValidateObj( m_vecFrameListeners );

	{
		AUTO_LOCK( m_MutexLayersToRepaint );
		ValidateObj( m_treeLayersToRepaint );
	}

	ValidateObj( m_vecPanelDestroyedDelegates );

	ValidateObj( m_mapV8PanelObjectTemplates );
	FOR_EACH_MAP_FAST( m_mapV8PanelObjectTemplates, i )
	{
		validator.ClaimMemory( m_mapV8PanelObjectTemplates[i] );
	}

	ValidateObj( m_mapV8ObjectTemplatesByType );
	FOR_EACH_MAP_FAST( m_mapV8ObjectTemplatesByType, i )
	{
		ValidateObj( m_mapV8ObjectTemplatesByType.Key( i ) );
		validator.ClaimMemory( m_mapV8ObjectTemplatesByType[i] );
	}


	ValidateObj( m_MapPanelV8Contexts );
	FOR_EACH_MAP_FAST( m_MapPanelV8Contexts, i )
	{
		validator.ClaimMemory( m_MapPanelV8Contexts[i] );
	}

	ValidateObj( m_mapOtherPanelsV8InContext );
	FOR_EACH_MAP_FAST( m_mapOtherPanelsV8InContext, i )
	{
		ValidatePtr( m_mapOtherPanelsV8InContext[i].GetPtr() );
	}

	ValidateObj( m_MapV8PanelObjectInstances );
	FOR_EACH_MAP_FAST( m_MapV8PanelObjectInstances, i )
	{
		validator.ClaimMemory( m_MapV8PanelObjectInstances[i] );
	}

	ValidateObj( m_MapV8GlobalObjectInstances );
	FOR_EACH_MAP_FAST( m_MapV8GlobalObjectInstances, i )
	{
		validator.ClaimMemory( m_MapV8GlobalObjectInstances[i] );
	}

	ValidateObj( m_vecV8GlobalFunctionRegistrations );
	FOR_EACH_VEC( m_vecV8GlobalFunctionRegistrations, i )
	{
		ValidateObj( m_vecV8GlobalFunctionRegistrations[i].m_strName );
		validator.ClaimMemory( m_vecV8GlobalFunctionRegistrations[i].m_pFunction );
	}

	ValidateObj( m_vecV8GlobalObjectRegistrations );
	FOR_EACH_VEC( m_vecV8GlobalObjectRegistrations, i )
	{
		ValidateObj( m_vecV8GlobalObjectRegistrations[i].m_strName );
		validator.ClaimMemory( m_vecV8GlobalObjectRegistrations[i].m_pObj );
	}

	ValidateObj( m_treeInFlightJSAsyncWebequestObjects );
	FOR_EACH_RBTREE_FAST( m_treeInFlightJSAsyncWebequestObjects, i )
	{
		ValidatePtr( m_treeInFlightJSAsyncWebequestObjects.Element( i  ) );
	}

	ValidateObj( m_MapInFlightJSONHTTPRequests );
	FOR_EACH_MAP_FAST( m_MapInFlightJSONHTTPRequests, i )
	{
		ValidateObj( m_MapInFlightJSONHTTPRequests[i].m_strURL );
	}

	ValidateObj( m_treeScheduledJSHandles );
	ValidateObj( m_HTTPRequestCompleted );
	ValidateObj( m_ListScheduledDelegates );
	ValidatePtr( m_pFileSystem );
	validator.ClaimMemory( m_pStyleFactory );
	ValidateObj( m_mapEventRegistrations );
	ValidateObj( m_mapPanelRegistrations );

	m_tslQueuedDecRef.ValidateDataStructureOnly( validator, "m_tslQueuedDecRef" );
	ValidateObj( m_MapV8IUIWindowObjectInstances );
	FOR_EACH_MAP_FAST( m_MapV8IUIWindowObjectInstances, i )
	{
		validator.ClaimMemory( m_MapV8IUIWindowObjectInstances[i] );
	}
	validator.ClaimMemory( m_pSoundSystem );
	
	CUtlSymbol::ValidateGlobals( validator, pchName );
}

#endif


