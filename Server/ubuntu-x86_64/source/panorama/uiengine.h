//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIENGINE_H
#define UIENGINE_H
#pragma once

#if defined( SOURCE2_PANORAMA ) && !defined( VERSION_SAFE_STEAM_API_INTERFACES )
#define VERSION_SAFE_STEAM_API_INTERFACES
#endif

#include "tier1/utlrbtree.h"


#include "panorama/iuipanel.h"
#include "input/uiinput.h"
#include "panorama/uisettings.h"
#include "panorama/text/iuitextservices.h"
#if defined( SOURCE2_PANORAMA )
#include "tier1/utldict.h"
#include "tier1/utlpriorityqueue.h"
#include "tier1/utlptr.h"
#include "fileio.h" // for CDirWatcher
#else
#include "constants.h"
#include "globals.h"
#include "framefunction.h"
#endif
#include "utlstring.h"
#include "utlmap.h"
#include "utllinkedlist.h"
#include "reliabletimer.h"
#include "utlmap.h"
#include "utlsortvector.h"
#include "tier0/tslist.h"
#include "steam/isteamhttp.h"
#include "steam/steam_api.h"
#ifdef PLATFORM_WINDOWS_PC
#include "socketlib/socketlib.h"
#endif

#include "../gcsdk/steamextra/tier1/utlstringbuilder.h"


#if defined( SOURCE2_PANORAMA )
#include "../thirdparty/v8/include/v8.h"
#include "../thirdparty/v8/include/v8.h"
#else
#include <html/ichromehtmlwrapper.h>
#include "../external/v8/include/v8.h"
#include "../external/v8/include/v8.h"
#endif

#include "ctx_debug.h"

#if V8_DEBUGGING_ENABLED
#define V8_DEBUGGER_AGENT_PORT 42000
#endif

#ifdef PANORAMA_USE_S1WRAPPER
extern void wrapper_panorama_reload( bool bForceReload );
#endif

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: maps a single steam async call result to a class member function
//			template params: T = local class, P = parameter struct
//-----------------------------------------------------------------------------
template< class T, class P >
class CMultipleCallResults : private CCallbackBase
{
public:
	typedef void (T::*func_t)(P*, bool);

	CMultipleCallResults( T *pObj, func_t func ) : m_pObj( pObj ), m_Func( func )
	{
		SetDefLessFunc( m_mapAPICalls );
		m_iCallback = P::k_iCallback;
	}

	void AddCall( SteamAPICall_t hAPICall )
	{
		m_mapAPICalls.Insert( hAPICall );
		SteamAPI_RegisterCallResult( this, hAPICall );
	}

	unsigned int GetNumActive() const
	{
		return m_mapAPICalls.Count();
	}

	void RemoveCall( SteamAPICall_t hAPICall )
	{
		if ( m_mapAPICalls.Remove( hAPICall ) )
		{
			SteamAPI_UnregisterCallResult( this, hAPICall );
		}
	}

	~CMultipleCallResults()
	{
		FOR_EACH_RBTREE_FAST( m_mapAPICalls, i )
		{
			SteamAPI_UnregisterCallResult( this, m_mapAPICalls.Element( i ) );
		}
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_mapAPICalls );
	}
#endif // DBGFLAG_VALIDATE

private:
	virtual void Run( void *pvParam )
	{
		(m_pObj->*m_Func)((P *)pvParam, false);
	}
	void Run( void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall )
	{
		if ( m_mapAPICalls.Remove( hSteamAPICall ) )
		{
			// run the callback
			(m_pObj->*m_Func)((P *)pvParam, bIOFailure);
		}
	}
	int GetCallbackSizeBytes()
	{
		return sizeof( P );
	}

	CUtlRBTree<SteamAPICall_t> m_mapAPICalls;
	T *m_pObj;
	func_t m_Func;
};

// utility macro for declaring the function and callback object together
#define STEAM_CALLRESULT( thisclass, name, param ) CMultipleCallResults< thisclass, param > m_##name; void On##name( param *pParam, bool bIOFailure )

#endif

#if defined( SOURCE2_PANORAMA )

class CSteamAPIContext;
extern CSteamAPIContext steamAPIContext;
#define ClientHTTP() (steamAPIContext.SteamHTTP())

#endif

#if V8_DEBUGGING_ENABLED
class InspectorClient;
class CWebsocketServer;
#endif

class CMemoryStack;

namespace panorama
{

class CTopLevelWindow;
class IUIEvent;
class CConsole;
class CLayoutManager;
class CUIInputEngine;
class CLocalization;
class IUISoundSystem;
class CDebugger;
class CUIPanel;
class CStyleFactoryWrapper;


struct RegisterJSEntryParamInfoInternal_t
{
	CUtlString strName;
	RegisterJSType_t eDataType;
};

struct RegisterJSEntryInfoInternal_t
{
	CUtlString strName;
	CUtlString strDescription;
	uint32 unFlags;
	RegisterJSType_t eDataType;
	uint8 unNumParams;
	RegisterJSEntryParamInfoInternal_t pParamInfos[RegisterJSEntryInfo_t::k_unMaxParams];
	
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( strName );
		ValidateObj( strDescription );
	}
#endif // DBGFLAG_VALIDATE
};

struct RegisterJSScopeInfoInternal_t
{
	CUtlString strName;
	CUtlString strDescription;
	CUtlVector<RegisterJSEntryInfoInternal_t> vecEntries;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( strName );
		ValidateObj( strDescription );
		ValidateObj( vecEntries );
		FOR_EACH_VEC( vecEntries, i )
		{
			ValidateObj( vecEntries[i] );
		}
	}
#endif // DBGFLAG_VALIDATE
};


// Central error routine for all Require* methods.
void RequiredCallFailed( const char *pchFormat, ... );


//-----------------------------------------------------------------------------
// Purpose: Event handler types
//-----------------------------------------------------------------------------
struct EventHandler_t
{
	EventHandler_t() : pContextPanel( NULL ), pjsHandler( NULL ), unHandlerId( 0 ) {}

	CPanoramaSymbol symEvent;
	CUtlAbstractDelegate pHandler;
	v8::Persistent<v8::Function> *pjsHandler;
	IUIPanel *pContextPanel;
	uint32 unHandlerId;
};
typedef CUtlVector< EventHandler_t > VecEventHandlers_t;

struct JSGenericCallback_t
{
	JSGenericCallback_t()
	: m_pContextPanel( NULL )
	, m_nCallbackHandle( panorama::JS_GENERIC_CALLBACK_HANDLE_INVALID )
	{
	}

	JSGenericCallback_t( panorama::JSGenericCallbackHandle_t nHandle, IUIPanel *pContextPanel, v8::Isolate *pIsolate, v8::Handle< v8::Function > jsHandler )
	: m_pContextPanel( pContextPanel )
	, m_jsHandler( pIsolate, jsHandler )
	, m_nCallbackHandle( nHandle )
	{
	}

	v8::Persistent< v8::Function > m_jsHandler;
	IUIPanel *m_pContextPanel;
	panorama::JSGenericCallbackHandle_t m_nCallbackHandle;
};
typedef CUtlVector< JSGenericCallback_t* > VecJSGenericCallbackPtr_t;



class CJSAsyncWebRequest;

//
// Base instance of UI engine, with non platform specific functionality
//
class CUIEngine : public IUIEngine
{
public:
	CUIEngine();
	virtual ~CUIEngine();

	// IUIEngine implementation, we implement some directly, require lower level classes to implement others.
	virtual void Shutdown();
	virtual void RequestShutdown() OVERRIDE { m_bShuttingdown = true; } 
#if defined( SOURCE2_PANORAMA )
	virtual bool StartupSubsystems( IUISettings *pSettings, PlatWindow_t hWindow );
#else
	virtual bool StartupSubsystems( IUISettings *pSettings, IHTMLChromeController *pHTMLController );
#endif
	virtual void ConCommandInit( IConCommandBaseAccessor *pAccessor ) OVERRIDE;
	virtual void Run() OVERRIDE;
	virtual void RunFrame() OVERRIDE;
	virtual void SetAggressiveFrameRateLimit( bool bLimitMainThread, bool bLimitRendering ) OVERRIDE;
	virtual bool BIsRunning() OVERRIDE;
	virtual bool BHasFocus() OVERRIDE;
	
	virtual bool BShouldUseForceBuiltPaintCmdCaches() OVERRIDE { return m_bUseForceBuiltPaintCmdCaches; }
	virtual void SetUseForceBuiltPaintCmdCaches( bool bUseForceBuiltPaintCmdCaches ) OVERRIDE 
	{ 
		m_bUseForceBuiltPaintCmdCaches = bUseForceBuiltPaintCmdCaches; 
	} 

#if !defined( SOURCE2_PANORAMA )
	virtual IUIWindow *CreateNewWindow( const char *pchWindowTitle, uint32 width, uint32 height, ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetWindow ) = 0;
	virtual IUIWindow *CreateNewOverlayWindow( const char *pchWindowTitle, uint32 width, uint32 height, panorama::IUIEngine::ERenderTarget eTarget, bool bFixedSize, bool bDrawCustomMouseCursor ) 
	{
		return CreateNewWindow( pchWindowTitle, width, height, eTarget, bFixedSize, true, bDrawCustomMouseCursor, "" );
	}
	IUIWindow *CreateNewOpenVROverlayWindow( uint32 width, uint32 height, vr::VROverlayHandle_t ulOverlayHandle, bool bKeepInputFocusOnGamepadFocusLost, bool bIgnoreGamepadFocus ) OVERRIDE;
#else
	virtual IUIWindow *CreateNewUILayerWindow( uint32 xPos, uint32 yPos, uint32 width, uint32 height, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, bool bAcceptKBandMouse, const char *pName, InputContextHandle_t hInputContext ) = 0;
	virtual IUIWindow *CreateNewOffscreenUIWindow( uint32 width, uint32 height, const char *pName, InputContextHandle_t hInputContext, bool bDrawToBackBuffer ) = 0;
	virtual bool DestroyWindow( IUIWindow *pWindow ) = 0;
#endif
	virtual void OnResolutionChange( float fRelativeScalefactor ) = 0;
	virtual void OnGPUMemLevelChanged() = 0;

	virtual IUITextLayout *CreateTextLayout( const char *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight );
	virtual IUITextLayout *CreateTextLayout( const uchar16 *pch16Text, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight );
	virtual IUITextLayout *CreateTextLayout( const uchar32 *pch32Text, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight );
	virtual void FreeTextLayout( IUITextLayout *pLayout );
	virtual const CUtlSortVector< CUtlString > &GetSortedValidFontNames();
	
	virtual double GetCurrentFrameTime() { return m_flCurrentFrameTime; }
	virtual void ReloadLayoutFile( CPanoramaSymbol symPath );
	virtual void AddFrameListener( IUIEngineFrameListener *pListener );
	virtual void RemoveFrameListener( IUIEngineFrameListener *pListener ) ;
	virtual bool BAnyWindowHasFocus();
	virtual bool BAnyVisibleWindowHasFocus();
	virtual bool BAnyOverlayWindowHasFocus();
	virtual IUIWindow *GetFocusedWindow( bool bSkipVRWindows = false );

	double GetLastInputTime() { return m_flLastInputTime; }
	void UpdateLastInputTime();

	virtual void RegisterFrameFunc( PanoramaFrameFunc_t frameFunc ) OVERRIDE;

	void LayoutAndPaintWindows();

	IUIInput *UIInputEngine();
    IUILocalization *UILocalize();
	IUISoundSystem *UISoundSystem();
	IUISettings *UISettings() { return m_pSettings; }
	IUIFileSystem *UIFileSystem()  { return m_pFileSystem; }

	// control/client access to layout manager
	virtual IUILayoutManager *UILayoutManager();
	
	// Framework internal
	CLayoutManager *UILayoutManagerInternal() { return m_pUILayoutManager; }

	// Render Command memory management
	CMemoryStack *AcquireRenderCommandMemoryStack();
	void ReleaseMemoryCommandStack( CMemoryStack *pMemoryStack );

	// panel management
	virtual IUIPanel *CreatePanel( IUIWindow *pWindow ) OVERRIDE;
	virtual void PanelDestroyed( IUIPanel *pPanel, IUIPanel *pOldParent ) OVERRIDE;
	virtual bool IsValidPanelPointer( const IUIPanel *pPanel ) OVERRIDE;
	virtual PanelHandle_t GetPanelHandle( const IUIPanel *pPanel ) OVERRIDE;
	virtual IUIPanel *GetPanelPtr( const PanelHandle_t &handle ) OVERRIDE;
	virtual void CallBeforeStyleAndLayout( IUIPanel *pPanel ) OVERRIDE;

		// registration for panel destroyed
	virtual void RegisterForPanelDestroyed( PanelDestroyedDel_t del ) OVERRIDE;
	virtual void UnregisterForPanelDestroyed( PanelDestroyedDel_t del ) OVERRIDE;

	// storage for mouse can activate info
	void RegisterMouseCanActivateParent( IUIPanel *pPanel, const char *pchParent );
	void UnregisterMouseCanActivateParent( IUIPanel *pPanel );
	const char *GetMouseCanActivateParent( IUIPanel *pPanel );

	// Message management
	virtual void RegisterEventHandler( CPanoramaSymbol symMsg, IUIPanel *pPanel, CUtlAbstractDelegate pFunc ) OVERRIDE;
	virtual void UnregisterEventHandler( CPanoramaSymbol symMsg, IUIPanel *pPanel, CUtlAbstractDelegate pFunc ) OVERRIDE;
	virtual void RegisterEventHandler( CPanoramaSymbol symMsg, IUIPanelClient *pPanel, CUtlAbstractDelegate pFunc ) OVERRIDE { return RegisterEventHandler( symMsg, pPanel->UIPanel(), pFunc );  }
	virtual void UnregisterEventHandler( CPanoramaSymbol symMsg, IUIPanelClient *pPanel, CUtlAbstractDelegate pFunc ) OVERRIDE { return UnregisterEventHandler( symMsg, pPanel->UIPanel(), pFunc ); }
	virtual void UnregisterEventHandlersForPanel( IUIPanel *pPanel ) OVERRIDE;
	virtual void RegisterForUnhandledEvent( CPanoramaSymbol symMsg, CUtlAbstractDelegate pFunc ) OVERRIDE;
	virtual void UnregisterForUnhandledEvent( CPanoramaSymbol symMsg, CUtlAbstractDelegate pFunc ) OVERRIDE;
	virtual void UnregisterForUnhandledEvents( void *pEventHandler ) OVERRIDE;
	virtual bool BHaveEventHandlersRegisteredForType( CPanoramaSymbol symPanelType ) OVERRIDE;
	virtual void RegisterPanelTypeEventHandler( CPanoramaSymbol symMsg, CPanoramaSymbol symPanelType, CUtlAbstractDelegate pFunc, bool bThisPtrIsUIPanel /* = false */ ) OVERRIDE;
	virtual bool DispatchEvent( IUIEvent *pEvent ) OVERRIDE;

	virtual void DispatchEventAsync( float flDelay, IUIEvent *pEvent ) OVERRIDE;
	virtual bool BAnyHandlerRegisteredForEvent( const CPanoramaSymbol &symEvent ) OVERRIDE;
	virtual CPanoramaSymbol GetLastDispatchedEventSymbol() OVERRIDE;
	virtual  IUIPanel *GetLastDispatchedEventTargetPanel() OVERRIDE;

	// Event filtering
	virtual void RegisterEventFilter( CUtlAbstractDelegate pFunc );
	virtual void UnregisterEventFilter( CUtlAbstractDelegate pFunc );

	virtual const char *GetApplicationInstallPath();
	virtual const char *GetApplicationUserDataPath();

	virtual void RegisterNamedLocalPath( const char *pathName, const char *pchLocalPath, bool bWatchForFileChanges, bool bAddToOverwriteIfExists = false );
	virtual void RegisterNamedUserPath( const char *pathName, const char *pchUserPath, bool bWatchForFileChanges, bool bAddToOverwriteIfExists = false );
	virtual void RegisterCustomFontPath( const char *pchFontPath );
	virtual const char *GetLocalPathForNamedPath( const char *pathName );
	virtual void GetLocalPathForRelativePath( const char *pchLocalPathName, const char *pchRelativePathname, CUtlString &strLocalPath );


	virtual void RegisterNamedRemoteHost( const char *namedRemoteHost, const char *pchRemoteHost );
	virtual void SetCookieHeaderForNamedRemoteHost( const char *namedRemoteHost, const CUtlVector<CUtlString> &vecCookies );
	virtual const CUtlVector<CUtlString> &GetCookieHeadersForNamedRemoteHost( const char *namedRemoteHost );
	virtual const char *GetRemoteHostForNamedHost( const char *namedRemoteHost );
	HTTPCookieContainerHandle GetCookieContainerForDomain( const char *pchHost );
	virtual bool BSetCookieForWebRequests( const char *pchHost, const char *pchPath, const char *pchCookie );
	virtual bool BClearCookieForWebRequests( const char *pchHost, const char *pchPath, const char *pchCookie );

	virtual void RegisterXHeader( const char *pchHeaderName, const char *pchHeaderValue );
	virtual int GetXHeaderCount() const;
	virtual void GetXHeader( int i, CUtlString &strName, CUtlString &strValue ) const;
	void AddCommonHeadersToHttpRequest( HTTPRequestHandle hRequest ) const;

	virtual void SetCookieHeaderForRemoteHost( const char *hostName, const CUtlVector<CUtlString> &vecCookies );
	virtual const CUtlVector<CUtlString> &GetCookieHeadersForRemoteHost( const char *hostName );
	virtual bool GetCookieValueForRemoteHost( const char *hostName, const char *cookieName, CUtlString *pstrCookieValue );

	// Turn on paint count tracking for panels
	virtual void SetPaintCountTrackingEnabled( bool bEnablePaintCountTracking );

	// Is paint count tracking on for panels
	virtual bool GetPaintCountTrackingEnabled() OVERRIDE { return m_bPaintCountTrackingEnabled; }

	// Increment paint count tracking for panels
	virtual void IncrementPaintCountForPanel( uint64 ulPanelPtrValue, bool bRequiredCompositionLayer, double flFrameTime ) OVERRIDE;

	// Get panel paint info for the panel
	virtual void GetPanelPaintInfo( uint64 ulPanelPtrValue, uint32 &unMaxPanelPaintCount, uint32 &unPaintCount, bool &bRequiredCompositionLayer, double &flFrameTimeLastPaint ) OVERRIDE;

	bool OnToggleDebug();
	bool OnShowPanelZoo();
	bool OnMemDump();
	bool OnWindowShutdown( IUIWindow *pWindow );

	CUtlLinkedList<CUtlString> &GetConsoleHistory() { return m_ConsoleHistory; }

	void WakePaintThread() { m_eventPaintThread.Set(); }
	// run a frame of the input engine (and therefore game controller
	void RunControllerFrame();

#if defined( SOURCE2_PANORAMA )
	ISteamHTMLSurface *AccessHTMLController();
#elif defined( PANORAMA_PUBLIC_STEAM_SDK )
	ISteamHTMLSurface *AccessHTMLController();
#else
	IHTMLChromeController *AccessHTMLController() { return m_pHTMLController; }
#endif

	// Clipboard access
	virtual void ClearClipboard() OVERRIDE;
	virtual void CopyToClipboard( const char *pchTextUTF8, const char *pchClipboardPasteStringLocToken ) OVERRIDE;
	virtual void GetClipboardText( CUtlString &strUTF8, CUtlString *out_psPasteStringLocToken ) const OVERRIDE;

	// Removing the one-to-one correspondence between context and panel.
	// Set the target panel for any JS. The function JSGetContextPanel(), called by JS to get the target panel, returns
	// the top of the panel stack. If a C++-side JS call is not bracketed by Push and Pop context, then the JS will
	// not get the correct target panel. 
	void PushContextPanel( IUIPanel *pPanel );
	void PopContextPanel( );

protected:
	// Internal interface
	virtual void CopyToClipboardImpl( const char *pchTextUTF8 ) = 0;
	virtual void GetClipboardTextImpl( CUtlString &strUTF8 ) const = 0;
	
public:
	// Overlay tracking
	bool BHasOverlayForApp( uint64 gameID, uint64 ulPID );
	void TrackOverlayForApp( uint64 gameID, uint64 ulPID, void * );
	void DeleteOverlayInstanceForApp( uint64 gameID, uint64 ulPID, void * );
	void *OverlayForApp( uint64 gameID, uint64 ulPID );

	// Pool allocations for panel styles
	virtual IUIPanelStyle *AllocPanelStyle( IUIPanel *pPanel ) OVERRIDE;
	virtual void FreePanelStyle( IUIPanelStyle *pStyle ) OVERRIDE;

	// Track panels with queued async delete
	virtual void SetPanelWaitingAsyncDelete( IUIPanel *pPanel );

	// Check if panel while still existing may be in a state where it is definately going to be deleted
	virtual bool BIsPanelWaitingAsyncDelete( IUIPanel *pPanel );

	// Pulse haptic feedback
	virtual void PulseActiveControllerHaptic( IUIEngine::EHapticFeedbackPosition ePosition, IUIEngine::EHapticFeedbackStrength eStrength );
	virtual EHapticFeedbackPosition GetHapticFeedbackPositionForInteraction();

	virtual void MarkLayerToRepaintThreadSafe( uint64 ulCompositionLayerID );

	virtual void AddDirectoryChangeWatch( const char *pchPath );

	// If you pass NULL script will run in UIEngine global context, should normally pass
	// the panel context within which the script should run
	virtual void RunScript( IUIPanel *pPanelContext, const char *pchScriptString, const char *pchSourceFileName, 
		int nSourceBeginLine, int nSourceBeginCol, bool bPrintRetValue, bool bIsReload ) OVERRIDE;

	// Expose a new object type/template to javascript with the given name, 
	// the function pointer passed should setup member accssors/methods with the functions
	// from uijsregistration.h
	virtual void ExposeObjectTypeToJavaScript( const char *pchObjectTypeName, CUtlAbstractDelegate &del ) OVERRIDE;

	// Is the object type name already exposed to JavaScript?
	virtual bool IsObjectTypeExposedToJavaScript( const char *pchObjectTypeName ) OVERRIDE;

	// Expose an instance of an object type as a global with specified name to javascript
	virtual void ExposeGlobalObjectToJavaScript( const char *pchJSVarName, void *pInstance, const char *pchJsTypeName, bool bTrueGlobal ) OVERRIDE;

	virtual void ClearGlobalObjectForJavaScript( const char *pchJSVarName, void *pInstance ) OVERRIDE;

	virtual void DeleteJSObjectInstance( IUIJSObject *pJSObjectInstance ) OVERRIDE;
	
	// Get panel that contains the javascript context
	virtual panorama::IUIPanel *GetPanelForJavaScriptContext( v8::Context *pContext ) OVERRIDE;

	// Get javascript context for a panel (may be NULL)
	virtual v8::Persistent<v8::Context> *GetJavaScriptContextForPanel( panorama::IUIPanel *pPanel ) OVERRIDE;

	// Helper to spew exceptions to console
	virtual void OutputJSExceptionToConsole( v8::TryCatch &try_catch, IUIPanel *pPanelContext ) OVERRIDE;

	// Add a function template to global namespace, by default this is really panorama., but you can specify to make it really global as well
	virtual void AddGlobalV8FunctionTemplate( const char *pchJSFuncName, v8::Handle< v8::FunctionTemplate > *pFunc, bool bTrueGlobal = false ) OVERRIDE;
	
	virtual v8::Persistent<v8::Context> &GetV8GlobalContext() OVERRIDE { return m_V8UIEngineGlobalContext; }

	// Helper to create JS object to wrap a given panel
	virtual v8::Persistent<v8::Object> *CreateV8PanelInstance( IUIPanel *pPanel ) OVERRIDE;

	// Helper to create a JS object to wrap a given panel style
	virtual v8::Persistent<v8::Object> *CreateV8PanelStyleInstance( IUIPanelStyle *pPanelStyle ) OVERRIDE;

	// Helper to create a JS object to wrap a given ui window
	virtual v8::Persistent<v8::Object> *CreateV8IUIWindowInstance( IUIWindow *pPanelStyle ) OVERRIDE;

	// Helper to create JS object for given js object type
	virtual v8::Persistent<v8::Object> *CreateV8ObjectInstance( const char *pchObjectType, void *pActualObject, IUIJSObject *pJSObject ) OVERRIDE;

	// Associates a panel with a javascript context (CPanel2D::SetPanelIntoContext())
	void AddPanelForV8Context( IUIPanel *pPanel, IUIPanel *pContext );
	void RemovePanelForV8Context( IUIPanel *pPanel, IUIPanel *pContext );
	CUtlVector< IUIPanel * > *GetAssociatedPanelsForV8Context( IUIPanel *pContext );
	void DeleteAssociatedPanelsForV8Context( IUIPanel *pContext );

	// Used internally by initialization code to register events with framework
	virtual void RegisterEventWithEngine( CPanoramaSymbol symEvent, UIEventFactory factory ) OVERRIDE;

	// Check if a symbol is a valid event name
	virtual bool IsValidEventName( const CPanoramaSymbol symEvent ) OVERRIDE;

	// Check if a symbol is a valid panel event name
	virtual bool IsValidPanelEvent( const CPanoramaSymbol symEvent, int *pParams ) OVERRIDE;

	// Create input event from symbol, internal framework use
	virtual IUIEvent *CreateInputEventFromSymbol( CPanoramaSymbol symEvent, IUIPanel *pPanel, EPanelEventSource_t eSource, int nRepeats ) OVERRIDE;

	// Create an event from a string representation
	virtual IUIEvent *CreateEventFromString( IUIPanel *pCreatingPanel, const char *pchEvent, const char **pchEventEnd ) OVERRIDE;

	// Create multiple events from a string representation (whitespace separated like you do in XML)
	virtual bool CreateEventsFromString( VecUIEvents_t *pOutVecUIEvents, IUIPanel *pCreatingPanel, const char *pchEvent, const char **pchEventEnd ) OVERRIDE;

	// Used internally by initialization code to register panels with framework
	virtual void RegisterPanelFactoryWithEngine( CPanoramaSymbol symPanelType, CPanel2DFactory *pFactory ) OVERRIDE;

	// Create debugger window
	virtual void CreateDebuggerWindow() OVERRIDE;

	// Close debugger window
	virtual void CloseDebuggerWindow() OVERRIDE;

	// Register any delegate to run at specified time, be sure to use CancelScheduledDelgate if you delete the object the delgate runs on, etc.
	virtual int RegisterScheduledDelegate( double flTargetFrameTime, CUtlDelegate< void() > del, const char *pchName ) OVERRIDE;

	// Cancel a scheduled delegate by index returned from RegisterScheduledDelegate
	virtual void CancelScheduledDelegate( int iScheduleIndex ) OVERRIDE;

	// Return the last frame time for which we already ran scheduled delegates
	virtual double GetLastScheduledDelegateRunTime() OVERRIDE { return m_flLastScheduledDelRunTime; }

	//
	// Public methods below here are available via UIEngineInternal(), but not UIEngine() as they
	// are not part of the IUIEngine interface.  This is designed to let you expose slightly more
	// internal oriented things that are needed in CUIPanel or UIEvent or something like that, but
	// which you don't want to expose to code outside panorama itself.
	//

	CUtlMap< CPanoramaSymbol, UIEventFactory, int, CDefLess< CPanoramaSymbol > > &MapRegisteredEvents() { return m_mapEventRegistrations; }

	void DeleteScriptContext( IUIPanel *pPanelContext );

	v8::Persistent<v8::Context> *GetContextForPanel( const IUIPanel *pPanel );
	
	// Compile some script, but keep it for later to run rather than running now
	v8::Persistent<v8::Script> *CompileScript( IUIPanel *pPanelContext, const char *pchScriptString, const char *pchSourceFileName );

	// If you pass NULL script will run in UIEngine global context, should normally pass
	// the panel context within which the script should run
	void RunScript( IUIPanel *pPanelContext, v8::Persistent<v8::Script> *pScript, bool bPrintRetValue );

	virtual v8::Handle< v8::Value > RunFunction( IUIPanel *pPanelContext, v8::Persistent<v8::Function> *pFunction, 
		int nNumArgs, v8::Handle<v8::Value> *pArgs, bool bPrintRetValue ) OVERRIDE;

	virtual v8::Handle< v8::Value > RunFunction( IUIPanel *pPanel, const char *pchFunctionName, 
		int nNumArgs, v8::Handle<v8::Value> *pArgs ) OVERRIDE;

	// Various code that uses JS needs this
	virtual v8::Isolate * GetV8Isolate() OVERRIDE { return m_pV8Isolate; }

	// Register/Unregister JS function as event handler, only used internal to UIEngine, other code must call RegisterEventHandler from within JS code
	uint32 RegisterJSEventHandler( CPanoramaSymbol symEvent, IUIPanel *pPanel, IUIPanel *pContextPanel, v8::Persistent< v8::Function > *pFunc );
	void UnregisterJSEventHandler( CPanoramaSymbol symEvent, IUIPanel *pPanel, uint32 unHandlerId );

	// Register JS function as event handler for unhandled event.
	uint32 RegisterJSForUnhandledEvent( CPanoramaSymbol symMsg, IUIPanel *pContextPanel, v8::Persistent< v8::Function > *pFunc );

	// Unregister JS function as event handler for unhandled event.
	void UnregisterJSForUnhandledEvent( CPanoramaSymbol symMsg, uint32 unHandlerId );

	// Find object template to use for returning given panel type to js as an object
	v8::Persistent<v8::FunctionTemplate> *GetJSClassTemplateForPanel( IUIPanel *pPanel );

	// Access the current class template we are setting up
	v8::Local<v8::FunctionTemplate> GetCurrentV8ClassTemplateToSetup() { return m_v8ClassTemplateSetupCur; }
	v8::Local<v8::ObjectTemplate> GetCurrentV8ObjectTemplateToSetup() { return m_v8ClassTemplateSetupCur->InstanceTemplate(); }
	v8::Local<v8::Signature> GetCurrentV8ClassToSetupSignature() { return m_v8ClassSignatureSetupCur; }
	v8::Local<v8::AccessorSignature> GetCurrentV8ClassToSetupAccessorSignature() { return m_v8ClassAccessorSignatureSetupCur; }

	// Direct access to input enginer
	CUIInputEngine *UIInputEngineInternal() { return m_pInputEngine; }

	// Allow access to style factory interface
	virtual IUIStyleFactory *UIStyleFactory();

	virtual uint32 GetWheelScrollLines() OVERRIDE
	{
		return 3;
	}

	// Create JSON web api job, use the helpers in uiwebapiclient.h directly instead, this is there for them to use internally
	virtual uint32 InitiateAsyncJSONWebAPIRequest( EHTTPMethod eMethod, CUtlString strURL, IUIPanel *pCallbackTargetPanel, void *pContext, CJSONWebAPIParams *pParams, HTTPCookieContainerHandle hCookieContainer ) OVERRIDE;

	// Create JSON web api job, use the helpers in uiwebapiclient.h directly instead, this is there for them to use internally
	virtual uint32 InitiateAsyncJSONWebAPIRequest( EHTTPMethod eMethod, CUtlString strURL, JSONWebAPIDelegate_t callback, void *pContext, CJSONWebAPIParams *pParams, HTTPCookieContainerHandle hCookieContainer ) OVERRIDE;

	// Cancel previously created web api request job
	virtual void CancelAsyncJSONWebAPIRequest( uint32 requestID ) OVERRIDE;

	// Resolves a path that may contain named path portions, etc, to a full local path
	virtual CUtlString ResolvePath( const char *pchPath ) OVERRIDE;

	// Is the panel type registered
	virtual bool BRegisteredPanelType( CPanoramaSymbol symPanelType ) OVERRIDE;

	// Factory func for creating panels
	virtual IUIPanelClient *CreatePanelClient( CPanoramaSymbol symName, const char *pchID, panorama::IUIPanel *parent ) OVERRIDE;

	// CPanoramaSymbol support for cross DLL symbols
	virtual UtlSymId_t MakeSymbol( const char *pchText ) OVERRIDE;

	// CPanoramaSymbol support for cross DLL symbols
	virtual const char * ResolveSymbol( const UtlSymId_t sym ) OVERRIDE;

	virtual ELanguage GetDisplayLanguage();
	
	void TrackAsyncJSWebRequest( CJSAsyncWebRequest *pRequest );
	void ClearAsyncJSWebRequest( CJSAsyncWebRequest *pRequest );

	// Interface to allow animation/render threads to queue a decrement of a ref count on an object next frame in the main thread
	virtual void QueueDecrementRefNextFrame( ::CRefCount *pRefCountObj ) OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
	bool PrepareForValidate();
	bool ResumeFromValidate();
#endif

	virtual JSGenericCallbackHandle_t RegisterJSGenericCallback( panorama::IUIPanel *pContextPanel, v8::Handle< v8::Function > callbackFunc ) OVERRIDE;
	virtual bool InvokeJSGenericCallback( JSGenericCallbackHandle_t nHandle, int nArgs, v8::Handle<v8::Value> *pArgs, v8::Handle< v8::Value > *pOutRetVal ) OVERRIDE;
	virtual void UnregisterJSGenericCallback( JSGenericCallbackHandle_t nHandle ) OVERRIDE;

	virtual int GetNumRegisterJSScopes() OVERRIDE;
	virtual void GetRegisterJSScopeInfo( int nScope, RegisterJSScopeInfo_t *pInfo ) OVERRIDE;
	virtual void GetRegisterJSEntryInfo( int nScope, int nEntry, RegisterJSEntryInfo_t *pInfo ) OVERRIDE;
	virtual int StartRegisterJSScope( const char *pName, const char *pDesc = NULL ) OVERRIDE;
	virtual void EndRegisterJSScope() OVERRIDE;
	virtual int NewRegisterJSEntry( const char *pName, uint32 unFlags, const char *pDesc = NULL, RegisterJSType_t eDataType = k_ERegisterJSTypeUnknown ) OVERRIDE;
	virtual void SetRegisterJSEntryParams( int nEntry, uint8 unNumParams, RegisterJSType_t *pParamTypes, const char *pchArgNames ) OVERRIDE;
	virtual bool BMatchDomainForJSRequest( IUIPanel *pContextPanel, const char *pchURL ) OVERRIDE;
	bool BMatchingDomainsForJSRequest( IUIPanel *pLHS, IUIPanel *pRHS );

	virtual void ClearFileCache() OVERRIDE;
	virtual void PrintCacheStatus() OVERRIDE;

	virtual void GetWindowsForDebugger( CUtlVector<IUIWindow *> &vecWindows ) OVERRIDE{} // no-op by default, only debug the main window
	virtual void OnFileCacheRemoved( CPanoramaSymbol fileSymbol ) { } // no-op by default

	virtual bool BHasAnyWindows() OVERRIDE { return m_vecWindows.Count() > 0 ? true : false; }

	v8::Local<v8::Value> RunJSFunctionInternal( IUIPanel *pPanelContext, v8::Handle<v8::Context> context,
		v8::Local<v8::Value> recv, v8::Local<v8::Function> jsfn, int argc, v8::Local<v8::Value> argv[], bool bPrintRetValue );

	v8::Local<v8::Value> RunJSScriptInternal( IUIPanel *pPanelContext, v8::Local<v8::Script> script, bool bPrintRetValue, bool bIsReload );

	bool IsReloadingScript( ) { return m_bIsReloadingScript; }

	// Helper for outputting javascript strings to the panorama debugger as well as the windows debugger
	void OutputJSString( const IUIPanel *pContext, const char *pchString, bool bException = false );

	uint GetNextScheduledJSHandle();
	bool BCancelScheduledJSHandle( uint hScheduled );

	enum EEventDocumentationType
	{
		k_eEventDocumentationType_All,
		k_eEventDocumentationType_Internal,
		k_eEventDocumentationType_External,
	};
	void DumpEventDocumentation( EEventDocumentationType eDocumentationType );

	void DrawEventStats( CUIRenderEngine * pRenderEngine );

protected:
	void CallQueuedPanelsBeforeStyleAndLayout();

	void CreatePanelZooWindow();
	void DispatchQueuedEvent( CLimitTimer &limit );
	void CreateConsoleWindow();

	void ToggleDebugMode();
	bool OnToggleConsole();
	bool OnDeletePanel( const CPanelPtr< IUIPanel > &pPanel );
	bool OnSetInputFocus( const CPanelPtr< IUIPanel > &ptrPanel );
	bool OnDropInputFocus( const CPanelPtr< IUIPanel > &ptrPanel );
	bool OnSetPanelSelected( const CPanelPtr< IUIPanel > &ptrPanel, bool bSelected );
	bool OnTogglePanelSelected( const CPanelPtr< IUIPanel > &ptrPanel );
	bool OnSetChildPanelsSelected( const CPanelPtr< IUIPanel > &ptrPanel, bool bSelected );
	bool OnCopyStringToClipboard( const CPanelPtr< IUIPanel > &ptrPanel, const char *pchString, const char *pszPasteLocToken );
	bool OnReloadStyleFile( CPanoramaSymbol symFile );
	bool OnSetAllChildrenActivationEnabled( const CPanelPtr< IUIPanel > &ptrPanel, bool bEnabled );
	bool OnAsyncEvent( float flDelay, IUIEvent * pEvent );

	bool OnJSScheduledFunction( CPanelPtr< IUIPanel > panelContext, v8::Persistent<v8::Function> *pJSFunc, int nLayoutReloadCount, uint hScheduled );
	bool DispatchJSEventHandler( IUIEvent *pEvent, const IUIPanel *pPanel, const EventHandler_t &handler );


	bool AutoReloadChangedFiles(); // helper function for file reloads
	virtual void ReloadChangedFile( const char *pchFile ) {} // method for subclasses to override
	bool OnReloadPanorama() { wrapper_panorama_reload( false ); return true; }
	bool OnForceReloadPanorama() { wrapper_panorama_reload( true ); return true; }


	// virtual RunPlatformFrame 
	virtual void RunPlatformFrame();
	virtual IUISoundSystem *CreateSoundSystem();

	void IncrementEventHandlerCount( const CPanoramaSymbol &symEvent, bool bUnhandledHandler, bool bPanelTypeHandler = false );
	void DecrementEventHandlerCount( const CPanoramaSymbol &symEvent, bool bUnhandledHandler );

	bool BIsEventFiltered( IUIEvent *pEvent );

	void InitializePanoramaContext( v8::Persistent<v8::Context> *pPersistentContext );

	CPanoramaSymbol GetPanelBaseClassSymbol( CPanoramaSymbol symPanelClass );

	void RunScheduledDelegates();

	// Create JSON web api job, use the helpers in uiwebapiclient.h directly instead, this is there for them to use internally
	uint32 InitiateAsyncJSONWebAPIRequestInternal( EHTTPMethod eMethod, CUtlString strURL, IUIPanel *pCallbackTargetPanel, JSONWebAPIDelegate_t callback, void *pContext, CJSONWebAPIParams *pParams, HTTPCookieContainerHandle hCookieContainer = INVALID_HTTPCOOKIE_HANDLE );

	void OnHTTPJSONWebAPIRequestFinished( HTTPRequestCompleted_t *pParam, bool bIOFailure );
	STEAM_CALLRESULT( CUIEngine, HTTPRequestCompleted, HTTPRequestCompleted_t );

	void RunQueuedDecRefCalls();

	// Have we been shutdown?
	bool m_bShutdown;
	// Should we shutdown now?
	bool m_bShuttingdown; 

	bool m_bUseForceBuiltPaintCmdCaches = 0;

	static bool s_bGlobalInitDone;
	static CThreadMutex s_MutexGlobalInit;
	static int s_nUIEnginesActive;
	
#if !defined( SOURCE2_PANORAMA )
	CMemoryPool m_PanelStylePool;
#else
	CUtlMemoryPool m_PanelStylePool;
#endif

	// main/paint thread waits on this at the end of run frame, lets animation thread wake us when we need to do work
	CThreadEvent m_eventPaintThread;

	// List of top level windows
	CUtlVector< CTopLevelWindow * > m_vecWindows;

	bool m_bInited;

	// Is there work remaining in the current frame?
	bool m_bWorkRemaining;

	// Did we already paint windows in the current frame?
	bool m_bPaintedWindows;

	bool m_bFrameTimerInited;			// Have we inited our frame timer (always true after the first frame)
	uint64 m_cFramesRun;				// How many frames have we run
	CLimitTimer m_LimitTimerFrame;		// Keep track of when this frame is done
	double m_flCurrentFrameTime;			// time at start of frame
	double m_flLastScheduledDelRunTime;	// last frame time at which we already ran scheduled delegates

	bool m_bAggressivelyLimitFrameRate;
	bool m_bAggressivelyLimitWindowFPS;

	// stuff we use to measure a frame
	CFastTimer m_fastTimerFrame; 

	// Last input time across all windows
	double m_flLastInputTime;
	// last time we sent a UserInputActive event
	double m_flLastUserActiveReportTime;

	bool m_bDebuggerActive;
	CTopLevelWindow *m_pConsoleWindow;
	CTopLevelWindow *m_pPanelZooWindow;

	CStyleFactoryWrapper *m_pStyleFactory;

	CUtlVector<PanoramaFrameFunc_t> m_vecFrameFuncs;

	// panel management
	CUtlMap< IUIPanel*, uint32, int, CDefLess< IUIPanel* > > m_mapPanels;		// contains all known panels and the current serial number. Used by PanelHandle_t to see if panel exists.
	static CInterlockedUInt m_unPanelSerialNumber;
	CUtlMap< IUIPanel*, CUtlString, int, CDefLess< IUIPanel* > > m_mapMouseCanActivateIfParent;	// map of all panels which can only be activated by mouse if parent has focus. Having this map here
																								// saves on an unnecessary CUtlString per panel
	CUtlVector< PanelDestroyedDel_t > m_vecPanelDestroyedDelegates;

	CUIInputEngine *m_pInputEngine;

	// Layout manager
	CLayoutManager *m_pUILayoutManager;

	// Render command memory management
#ifdef PANORAMA_USE_S1WRAPPER
	CTSList< CMemoryStack * > m_listAvailableCommandMemoryStacks;
#else
	CTSItemList< CMemoryStack * > m_listAvailableCommandMemoryStacks;
#endif

	// Message management
	CUtlHashMap< CPanoramaSymbol, VecEventHandlers_t*, CDefEquals< CPanoramaSymbol > > m_mapUnhandledEventHandlers;
	CUtlHashMap< void *, CUtlVector< CPanoramaSymbol >* > m_mapUnhandledEventHandlerMessages;
	CUtlMap< IUIPanel *, VecEventHandlers_t*, int, CDefLess< IUIPanel * > > m_mapPanelToJSUnhandledEventHandlers;
	uint32 m_unNextEventHandlerId;

	// Generic JS callbacks
	CUtlMap< IUIPanel *, VecJSGenericCallbackPtr_t*, int, CDefLess< IUIPanel * > > m_mapPanelToJSGenericCallbacks;
	CUtlMap< JSGenericCallbackHandle_t, JSGenericCallback_t*, int, CDefLess< JSGenericCallbackHandle_t > > m_AllJSGenericCallbacks;
	JSGenericCallbackHandle_t m_nNextGenericCallbackHandle;

	// Registered JS functions/methods/accessors.
	CUtlVector<RegisterJSScopeInfoInternal_t> m_vecRegisterJSScopes;
	int m_nCurRegisterJSScope;

	struct HandlerCount_t
	{
		HandlerCount_t() { m_nPanelHandlers = 0; m_nUnhandledHandlers = 0; m_nPanelTypeHandlers = 0; }

		int m_nPanelHandlers;
		int m_nUnhandledHandlers;
		int m_nPanelTypeHandlers;
	};
	CUtlHashMap< CPanoramaSymbol, HandlerCount_t, CDefEquals< CPanoramaSymbol > > m_mapEventsToHandlerCounts;

	// Event filters
	typedef CUtlVector< CUtlAbstractDelegate > VecEventFilters_t;
	VecEventFilters_t m_vecEventFilters;

	struct QueuedEvent_t
	{
		double flDispatch;			// time when message should be posted
		IUIEvent *pEvent;

		// sort by when these should be dispatched
		bool operator< ( const QueuedEvent_t &rhs ) const { return flDispatch < rhs.flDispatch; }
	};
	static bool QueuedMsgSort( const CUIEngine::QueuedEvent_t &lhs, const CUIEngine::QueuedEvent_t &rhs, void *pCtx );
	CUtlSortVector< QueuedEvent_t > m_vecQueuedEvents;
	CTSQueue<QueuedEvent_t> m_tslNewAsyncEvents;

	CTSQueue< ::CRefCount * > m_tslQueuedDecRef;

	CUtlLinkedList<CUtlString> m_ConsoleHistory;

	CLocalization *m_pLocalization;
	IUISoundSystem *m_pSoundSystem;

	// queue of panels which need need to do work before styles are applied & the layout pass
	CUtlRBTree< CPanelPtr< IUIPanel >, int, CDefLess< CPanelPtr< IUIPanel > > > m_treeCallBeforeStyleAndLayout;

	// tree of panesl waiting async delete
	CUtlRBTree< IUIPanel *, int, CDefLess< IUIPanel * > > m_treePanelsWaitingAsyncDelete;

	// Static members we'll cache application install and userdata paths to
	CUtlString m_strAppInstallPath;
	CUtlString m_strAppUserDataPath;

	// Registration dictionary for defined path names
	CUtlDict< CUtlString, short > m_dictNamedPaths;
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlMap< CUtlString, CUtlVector< CUtlString > *, int, CDefCaselessStringLess > m_mapNamedOverwritePaths;
#else
	CUtlMap< CUtlString, CUtlVector< CUtlString > *, int, CDefLess< CUtlString > > m_mapNamedOverwritePaths;
#endif

	struct XHeader_t
	{
		CUtlString strName;
		CUtlString strValue;
	};

	// Registration dictionary for additional X-headers
	CUtlVector< XHeader_t > m_vecXHeaders;

	struct NamedHost_t
	{
		CUtlString m_strHost;
		CCopyableUtlVector<CUtlString> m_vecCookieHeaders;
	};

#ifdef PANORAMA_USE_S1WRAPPER
	CUtlMap< CUtlString, NamedHost_t, int, CDefCaselessStringLess > m_mapNamedHosts;
	CUtlMap< CUtlString, CCopyableUtlVector<CUtlString>, int, CDefCaselessStringLess > m_mapHostCookies;
	CUtlMap< CUtlString, HTTPCookieContainerHandle, int, CDefCaselessStringLess > m_mapDomainCookieContainers;
#else
	CUtlMap< CUtlString, NamedHost_t, int, CDefLess< CUtlString > > m_mapNamedHosts;
	CUtlMap< CUtlString, CCopyableUtlVector<CUtlString>, int, CDefFastCaselessStringLess > m_mapHostCookies;
	CUtlMap< CUtlString, HTTPCookieContainerHandle, int, CDefFastCaselessStringLess > m_mapDomainCookieContainers;
#endif


	static CUtlVector< CUtlString >	sm_vecEmptyCookieList;

	struct DirWatchers_t
	{
		CDirWatcher m_dirWatcher; // global file watcher for our resource folder
		CUtlString m_sFullPath; // full file path to this watchers root
	};

	CUtlVector<DirWatchers_t*> m_vecDirWatchers;

#if !defined( SOURCE2_PANORAMA )
	// interface to create and delete html objects
	IHTMLChromeController *m_pHTMLController;
#endif

	CUtlVector< IUIEngineFrameListener * > m_vecFrameListeners;
	
	// container's settings interface - not lifetimed by us
	IUISettings *m_pSettings;

	struct OverlayInstance_t
	{
		uint64 gameID;
		uint64 ulPID;

		bool operator< ( const OverlayInstance_t &rhs ) const 
		{ 
			if ( gameID < rhs.gameID ) 
				return true;
			else if ( gameID > rhs.gameID )
				return false;

			return ulPID < rhs.ulPID;
		}
	};
	CUtlMap< OverlayInstance_t, void *, int, CDefLess< OverlayInstance_t > > m_mapOverlayInstances;

	CThreadMutex m_MutexLayersToRepaint;
	CUtlRBTree< uint64, int, CDefLess< uint64 > > m_treeLayersToRepaint;

	CPanoramaSymbol m_symLastDispatchedEvent;
	IUIPanel *m_pLastDispatchedEventTargetPanel;

	uint64 m_ulFramesTimeWentBackward;

	static v8::ArrayBuffer::Allocator *s_V8ArrayBufferAllocator;
	bool m_bDoV8GarbageCollect;
	double m_flLastV8IncrementalGC;
	v8::Isolate * m_pV8Isolate;
	v8::Persistent<v8::Context> m_V8UIEngineGlobalContext;

	v8::Persistent<v8::ObjectTemplate> m_V8GlobalTemplate;
	v8::Persistent<v8::ObjectTemplate> m_V8PanoramaTemplate;
	v8::Persistent<v8::ObjectTemplate> m_V8PanelStyleTemplate;

	v8::Local<v8::FunctionTemplate> m_v8ClassTemplateSetupCur;
	v8::Local<v8::Signature> m_v8ClassSignatureSetupCur;
	v8::Local<v8::AccessorSignature> m_v8ClassAccessorSignatureSetupCur;

	CUtlMap< CPanoramaSymbol, v8::Persistent<v8::FunctionTemplate> *, int, CDefLess< CPanoramaSymbol > > m_mapV8PanelClassTemplates;
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlMap< CUtlString, v8::Persistent<v8::FunctionTemplate> *, int, CDefCaselessStringLess > m_mapV8ClassTemplatesByType;
#else
	CUtlMap< CUtlString, v8::Persistent<v8::FunctionTemplate> *, int, CDefLess< CUtlString > > m_mapV8ClassTemplatesByType;
#endif
	CUtlMap< IUIPanel *, v8::Persistent<v8::Context> *, int, CDefLess< IUIPanel *> > m_MapPanelV8Contexts;
	CUtlMap< IUIPanel *, v8::Persistent<v8::Object> *, int, CDefLess< IUIPanel * > > m_MapV8PanelObjectInstances;
	CUtlMap< IUIPanel *, v8::Persistent<v8::Object> *, int, CDefLess< IUIPanel * > > m_MapV8PanelStyleObjectInstances;
	CUtlMap< IUIWindow *, v8::Persistent<v8::Object> *, int, CDefLess< IUIWindow * > > m_MapV8IUIWindowObjectInstances;
	CUtlMap< IUIPanel *, CUtlPtr< CUtlVector< IUIPanel* > >, int, CDefLess< IUIPanel * > > m_mapOtherPanelsV8InContext;
	
	CUtlMap< void *, v8::Persistent<v8::Object> *, int, CDefLess< void * > > m_MapV8GlobalObjectInstances;

	struct V8GlobalFunctionRegistration_t
	{
		CUtlString m_strName;
		v8::Persistent<v8::FunctionTemplate> *m_pFunction;
		bool m_bTrueGlobal;
	};

	CUtlVector<V8GlobalFunctionRegistration_t> m_vecV8GlobalFunctionRegistrations;

	struct V8GlobalObjectRegistration_t
	{
		CUtlString m_strName;
		v8::Persistent<v8::Object> *m_pObj;
		bool m_bTrueGlobal;
	};

	CUtlVector<V8GlobalObjectRegistration_t> m_vecV8GlobalObjectRegistrations;

	struct PanelTypeEventHandler_t
	{
		CUtlAbstractDelegate del;
		bool m_bIsUIPanelThisPtr;
	};
	CUtlHashMap< CPanoramaSymbol, CUtlHashMap< CPanoramaSymbol, PanelTypeEventHandler_t, CDefEquals< CPanoramaSymbol > > *, CDefEquals< CPanoramaSymbol > > m_mapPanelTypeEventHandlers;

	CUtlMap< CPanoramaSymbol, UIEventFactory, int, CDefLess< CPanoramaSymbol > > m_mapEventRegistrations;
	CUtlMap< CPanoramaSymbol, CPanel2DFactory*, int, CDefLess< CPanoramaSymbol > > m_mapPanelRegistrations;

	struct ScheduledItem_t
	{
		double m_flFrameTime; // time at which to run this item
		int m_iListIndex;	 // index into scheduled function array
#if !defined( SOURCE2_PANORAMA )
		CUtlString m_sName; // friendly name of the function being scheduled
#endif

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const char *pchName )
		{
			VALIDATE_SCOPE();
#if !defined( SOURCE2_PANORAMA )
			ValidateObj( m_sName );
#endif
		}
#endif // DBGFLAG_VALIDATE

	};
	static bool ScheduledItemSortFunc( ScheduledItem_t const &, ScheduledItem_t const & );

	bool m_bFrameFuncHasRun;

	CUtlPriorityQueue<ScheduledItem_t> m_QueueScheduledDelegates;
	CUtlLinkedList<CUtlDelegate< void() >, int> m_ListScheduledDelegates;

	CUtlRBTree< int, int, CDefLess< int > > m_treeScheduledJSHandles;
	uint m_unNextScheduledJSHandle;

	IUIFileSystem *m_pFileSystem;
	struct JSONWebAPIRequestInFlight_t
	{
		CPanelPtr<IUIPanel> m_TargetPanel;
		JSONWebAPIDelegate_t m_callback;
		void *m_pContext;
		bool m_bCanceled;
		CUtlString m_strURL;
	};
	CUtlMap< HTTPRequestHandle, JSONWebAPIRequestInFlight_t, int, CDefLess< HTTPRequestHandle > > m_MapInFlightJSONHTTPRequests;
	CUtlRBTree< CJSAsyncWebRequest *, int, CDefLess< CJSAsyncWebRequest *> > m_treeInFlightJSAsyncWebequestObjects;

	struct PanelPaintCount_t
	{
		PanelPaintCount_t()
		{
			m_unPaintsSinceReset = 0;
			m_flLastPaintTime = 0.0f;
			m_bLastNeededCompositionLayer = false;
		}
		uint32 m_unPaintsSinceReset;
		double m_flLastPaintTime;
		bool m_bLastNeededCompositionLayer;
	};

	CThreadMutex m_MutexPanelPaintCounts;
	CUtlMap< uint64, PanelPaintCount_t, int, CDefLess< uint64 > > m_MapPanelPaintCounts;
	uint32 m_unMaxPanelPaintsSinceReset;
	bool m_bPaintCountTrackingEnabled;

#if V8_DEBUGGING_ENABLED

public:

	void CaptureJSStackTrace( bool bPrint = false ) OVERRIDE;

	void EnableRemoteDebugger();
	void DisableRemoteDebugger();

	void DebuggerFrontEndConnected();
	void DebuggerFrontEndDisconnected();

	void DebuggerRunFrame();

private:

	bool IsWebSocketServerConnected();

	bool m_bDebugFrontEndDisconnected = false;
	InspectorClient *m_pInspectorClient = nullptr;
	CWebsocketServer *m_pWebsocketServer = nullptr;

#endif	// V8_DEBUGGING_ENABLED

private:
	uint32 m_unClipboardHash;
	CUtlString m_sClipboardPasteStringLocToken;

	// Set to true by RunScript before running script, if script is being reloaded. js can check for this during init
	// and restore any state that might help to continue iteration without having to restart the game
	bool m_bIsReloadingScript = false;

	v8::Persistent<v8::Context> *GetContextForPanelInternal( IUIPanel *pPanel );

	void ValidateJSFunction( v8::Persistent< v8::Function > *pFunc );
};

inline CUIEngine * UIEngineInternal() { return (CUIEngine*)UIEngine(); }

// Text services can be built in, such as in the Steam build,
// or in a separate binary, such as in the source2 build.
IUITextServices *UITextServices();
// On source2 this is part of the interfaces system.
#if !defined( SOURCE2_PANORAMA ) 
extern IUITextServices *g_IUITextServices;
#endif

} // namespace panorama

#endif // UIENGINE_H
