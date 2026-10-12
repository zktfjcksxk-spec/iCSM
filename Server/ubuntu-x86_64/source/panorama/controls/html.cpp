//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "mathlib/beziercurve.h"
#include "mathlib/vector.h"
#include "panorama/controls/html.h"
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
#define SteamHTMLSurface() UIEngine()->AccessHTMLController()
#else
#include "html/htmlprotobuf.h"
#include "html/htmlmessages.pb.h"
#include "html/ichromehtmlwrapper.h"
#endif
#include "jpegloader.h"
#include "controls/image.h"
#include "panorama/panoramacurves.h"
#include "pngloader.h"
#include "localization/ilocalize.h"
#include "renderer/styleproperties.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/controls/fileopendialog.h"
#include "panorama/uijsregistration.h"

#include "isteamcontroller.h"
#ifndef SOURCE2_PANORAMA
#include "vrapi.h"
#endif

#ifdef WIN32
#include "winlite.h"
#endif

#include <math.h>			// copysign

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CHTML, HTML );
REGISTER_PANEL2D_FACTORY( CHTMLSimpleNavigationWrapper, HTMLSimpleNavigationWrapper );
REGISTER_PANEL2D( CHTML::CHTMLVerticalScrollBar, HTMLVerticalScrollBar );
REGISTER_PANEL2D( CHTML::CHTMLHorizontalScrollBar, HTMLHorizontalScrollBar );

DEFINE_PANORAMA_EVENT( HTMLURLChanged ); // fired when a new page is starting to load in the html control or the url changed due to a redirect or the like
DEFINE_PANORAMA_EVENT( HTMLLoadPage ); // fired when a new page is starting to load in the html control
DEFINE_PANORAMA_EVENT( HTMLFinishRequest ); // fired when a page load fully finishes (i.e images, etc are also loaded)
DEFINE_PANORAMA_EVENT( HTMLTitle ); // fired when we get told the title to use for the page
DEFINE_PANORAMA_EVENT( HTMLStatusText ); // fired when cef has new status text to display, things like loading of images in the page
DEFINE_PANORAMA_EVENT( HMTLLinkAtPosition ); // fired when we get a return message about what link is located at this position, from an earlier RequestLinkAtPosition/RequestLinkUnderGamepad call
DEFINE_PANORAMA_EVENT( HTMLJSAlert ); // show an alert dialog to the user, must call CHTML::DismissJSAlert() when the user closes the alert dialog
DEFINE_PANORAMA_EVENT( HTMLJSConfirm ); // show a confirmation dialog to the user, must call CHTML::DismissJSConfirm( bool retVal ( when the user picks a choice
DEFINE_PANORAMA_EVENT( HMTLThumbNailImage ); // a thumbnail image was taken, this has the BGRA texture data for it for you to use
DEFINE_PANORAMA_EVENT( HTMLOpenLinkInNewTab ); // the user requested this link to open in a new tab
DEFINE_PANORAMA_EVENT( HTMLOpenPopupTab ); // the web page is requesting that a popup html window be made with this as parent
DEFINE_PANORAMA_EVENT( HTMLBackForwardState ); // sent then the enabled state of the back or forward buttons change
DEFINE_PANORAMA_EVENT( HTMLUpdatePageSize ); // size of the html container has changed
DEFINE_PANORAMA_EVENT( HTMLSecurityStatus ); // fired when a page is loaded the security status for this page
DEFINE_PANORAMA_EVENT( HTMLFullScreen ); // fired when entering or exiting fullscreen
DEFINE_PANORAMA_EVENT( HTMLStartMousePanning ); // when the user starts using mouse panning move
DEFINE_PANORAMA_EVENT( HTMLStopMousePanning ); // when the user stops panning
DEFINE_PANORAMA_EVENT( HTMLCloseWindow ); // the browser requested this window to be closed
DEFINE_PANORAMA_EVENT( HTMLFormHasFocus ); // fired when an input control on the page gets focus
DEFINE_PANORAMA_EVENT( HTMLScreenShotTaken ); // fired when a screenshot for a control is taken
DEFINE_PANORAMA_EVENT( HTMLFocusedNodeValue ); // the reply from RequestFocusedNodeValue
DEFINE_PANORAMA_EVENT( HTMLStartRequest ); // fired when a page load is requested, set the bool param to false to block the load

DECLARE_PANEL_EVENT0( HTMLRequestRepaint );
DEFINE_PANORAMA_EVENT( HTMLRequestRepaint );


DECLARE_PANEL_EVENT2( HTMLScreenShotCaptured, int, int );
DEFINE_PANORAMA_EVENT( HTMLScreenShotCaptured );

DECLARE_PANEL_EVENT0( HTMLFormFocusPending )
DEFINE_PANORAMA_EVENT( HTMLFormFocusPending ); // internal event to detect key focus not moving

DECLARE_PANEL_EVENT1( HTMLCommitZoom, float );
DEFINE_PANORAMA_EVENT( HTMLCommitZoom );


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
// helper to send IPC messages to the CEF thread
#define DISPATCH_MESSAGE( eCmd ) \
	cmd.Body().set_browser_handle( m_iBrowser );\
	HTMLCommandBuffer_t *pBuf = UIEngine()->AccessHTMLController()->GetFreeCommandBuffer( eCmd, m_iBrowser ); \
	cmd.SerializeCrossProc( &pBuf->m_Buffer, pBuf->m_eCmd, pBuf->m_iBrowser ); \
	if ( m_iBrowser == -1 ) { m_vecPendingMessages.AddToTail( pBuf ); } \
	else { \
		UIEngine()->AccessHTMLController()->PushCommand( pBuf ); \
		UIEngine()->AccessHTMLController()->ReleaseCommandBuffer( pBuf ); \
	}
#endif

#if defined ( PANORAMA_USE_S1WRAPPER )
ISteamController *SteamController()
{
	return NULL;
}
#endif


#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
ISteamController *ClientControllerLocal() { return SteamController(); }
#endif

// Zero-init gives us a "" uchar32 string.
uchar32 CHTML::s_ch32EmptyStr[1];

//
// assorted constants used by the html control, mostly tuning variables
//
namespace panorama
{
	const float k_flScrollLeadInTimeAnalog = 1.0f; // lean in time for analog stick scrolls
	const float k_flZoomlLeadInTime = 0.5f; // amount of time to scale scrolling speed on first move
	const float k_fScrollDeadzoneScale = 0.5f; // The typical dead zone is too large, lets halve it, playing with a controller this still feels good
	
	const float k_flIgnoreMouseAfterGamepadInputTimer = 2.0f; // ignore mouse input to the html control for this long after the last gamepad input use
	uint32 CHTML::sm_PaintCount = 0; // number of paints since GetAndResetPaintCounter was last called
	const float k_flZoomIncrement = 0.02f; // the increment to zoom into/out of the page by when using the right stick
	const float k_flZoomIncrementKeyBoard = 0.2f; // the increment to zoom into/out of the page by when using the keyboard
	const float k_flZoomMinimum = 0.8f; // the increment to zoom into/out of the page by when using the right stick
	const int k_nThumbNailWide = 400;
	const int k_nThumbNailTall = 300;
	const float k_flKeyScrollSpeedMultiplier = 5; // multipler * key repeat count for key scroll speed
	const float k_flMouseWheelZoomSpeedModifier = 1/100.0f; // amount to increase/decrease zoom by for each delta click of the mouse wheel
	const float k_flExtraPageScroll = 10.0f; // how many extra pixels to allow scroll beyond the page edge, that bounces back after stick release
	const float k_flMousePanningDeadZone = 5.0f; // number of pixel to ignore for mouse panning
	const float k_flMousePanningMaxSpeed = 10.0f; // max speed to allow mouse panning scroll
	const float k_flMousePanningSpeedModifier = 0.2f; // pixel offset times this to translate to scroll speed
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	static const char *pchYouTubeCookieURL = "http://www.youtube.com"; // cookie url we use to opt into the html5 youtube beta
	static const char *pchVimeoCookieURL = "http://vimeo.com"; // cookie url we use to opt into the html5 vimeo beta
#endif
	const float k_flFormFocusDelayCheck = 0.5f; // time to manually fire a form focus event if CEF hasn't told us key focus changed
	const float k_flMaxZoomLevel = 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: A wrapper panel for the actual html texture, this is a child of the html control
//-----------------------------------------------------------------------------
class CTexturePanel : public CPanel2D
{
public:
	CTexturePanel( CHTML *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
	{
		m_pParent = pParent;

		SetPosition( CUILength( 0.0f, CUILength::k_EUILengthLength ), CUILength( 0.0f, CUILength::k_EUILengthLength ), CUILength( 0.0f, CUILength::k_EUILengthLength ) );
		SetSize( CUILength( 100.0f, CUILength::k_EUILengthPercent ), CUILength( 100.0f, CUILength::k_EUILengthPercent ) );
		AccessStyleDirty()->SetOverflow( k_EOverflowNoClip, k_EOverflowNoClip );
	}

	virtual EMouseCursors GetMouseCursor() OVERRIDE
	{
		return m_pParent->BIsCursorOverLink() ? eMouseCursor_Hand : eMouseCursor_Arrow ;
	}

	~CTexturePanel() {}

	void Paint()
	{
		if ( m_pParent->m_pDoubleBufferedTexture && m_pParent->m_nTextureSerial >= 0 ) // only paint if we have a texture uploaded
		{
			const float u0 = 0.0f, u1 = 1.0f , v0 = 0.0f, v1 = 1.0f;
			float flY = 0.0f, flX = 0.0f;

			flX = floor(m_pParent->m_ScrollLeft.m_flOffsetTextureScroll);
			flY = floor(m_pParent->m_ScrollUp.m_flOffsetTextureScroll);

			// always draw a filled, white rect as the background of the image, web pages expect an opaque black backing layer or won't composite well
			uint32 unBackgroundColor = Color( 0x0, 0x0, 0x0, 0xff ).AsUint32();
			AccessRenderEngine()->DrawSolidColorRect( flX, flY, m_pParent->m_nHTMLPageWide + flX, m_pParent->m_nHTMLPageTall + flY, unBackgroundColor, k_EAntialiasingNone );

			AccessRenderEngine()->DrawSyncronizedTexturedRect( m_pParent->m_pDoubleBufferedTexture, k_ETextureSampleModeNormal, m_pParent->m_nTextureSerial, flX, flY,
				m_pParent->m_nTextureWide + flX, m_pParent->m_nTextureTall + flY, u0, v0, u1, v1 );
		}

		if ( m_pParent->m_bPopupVisible && m_pParent->m_pDoubleBufferedTextureComboBox )
		{
			if ( m_pParent->m_ComboTexture.TellPut() > 0 && m_pParent->m_pDoubleBufferedTextureComboBox->BIsReady() )
			{
				m_pParent->m_nTextureSerialCombo = m_pParent->m_pDoubleBufferedTextureComboBox->UpdateTextureData((void *)m_pParent->m_ComboTexture.Base() );
				m_pParent->m_ComboTexture.Clear();
			}

			if ( m_pParent->m_nTextureSerialCombo >= 0 )
			{
				const float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
				float flY, flX;
				flX = m_pParent->m_nPopupX;
				flY = m_pParent->m_nPopupY;

				AccessRenderEngine()->DrawSyncronizedTexturedRect( m_pParent->m_pDoubleBufferedTextureComboBox, k_ETextureSampleModeNormal, m_pParent->m_nTextureSerialCombo, flX, flY,
					flX + m_pParent->m_nPopupWide, flY + m_pParent->m_nPopupTall, u0, v0, u1, v1);
			}
		}
	}

private:
	CHTML *m_pParent;
};


#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
#define HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER 1
//-----------------------------------------------------------------------------
// Purpose: Global callbacks for Steam Web Surface interface, see Steamworks SDK docs:
//
// YOU MUST HAVE IMPLEMENTED HANDLERS FOR HTML_BrowserReady_t, HTML_StartRequest_t,
// HTML_JSAlert_t, HTML_JSConfirm_t, and HTML_FileOpenDialog_t! See the CALLBACKS
// section of this interface (AllowStartRequest, etc) for more details. If you do
// not implement these callback handlers, the browser may appear to hang instead of
// navigating to new pages or triggering javascript popups.
//
// We allocate it only once upon first instance of HTML control web surface and let
// it leak upon shutdown. This class guarantees that callbacks are properly responded
// to even if the game client side HTML surface object was destroyed/released.
//-----------------------------------------------------------------------------
class CHtmlSteamWebSurfaceGlobalCallbacksListener
{
public:
	STEAM_CALLBACK( CHtmlSteamWebSurfaceGlobalCallbacksListener, OnBrowserReady, HTML_BrowserReady_t, m_HTML_BrowserReady );
	STEAM_CALLBACK( CHtmlSteamWebSurfaceGlobalCallbacksListener, OnHTMLStartRequest, HTML_StartRequest_t, m_HTML_StartRequest );
	STEAM_CALLBACK( CHtmlSteamWebSurfaceGlobalCallbacksListener, OnHTMLJSAlert, HTML_JSAlert_t, m_HTML_JSAlert );
	STEAM_CALLBACK( CHtmlSteamWebSurfaceGlobalCallbacksListener, OnHTMLJSConfirm, HTML_JSConfirm_t, m_HTML_JSConfirm );
	STEAM_CALLBACK( CHtmlSteamWebSurfaceGlobalCallbacksListener, OnHTMLFileOpenDialog, HTML_FileOpenDialog_t, m_HTML_FileOpenDialog );

	CHtmlSteamWebSurfaceGlobalCallbacksListener()
		: m_HTML_BrowserReady( this, &CHtmlSteamWebSurfaceGlobalCallbacksListener::OnBrowserReady )
		, m_HTML_StartRequest( this, &CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLStartRequest )
		, m_HTML_JSAlert( this, &CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLJSAlert )
		, m_HTML_JSConfirm( this, &CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLJSConfirm )
		, m_HTML_FileOpenDialog( this, &CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLFileOpenDialog )
	{
	}

	CUtlRBTree< HHTMLBrowser, int, CDefLess< HHTMLBrowser > > m_rbKnownBrowserCallbackHandlers;
}
* g_pHtmlSteamWebSurfaceGlobalCallbacksListener = NULL;

void CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLJSAlert( HTML_JSAlert_t *p )
{
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->JSDialogResponse( p->unBrowserHandle, false );
	}
}
void CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLJSConfirm( HTML_JSConfirm_t *p )
{
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->JSDialogResponse( p->unBrowserHandle, false );
	}
}
void CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLFileOpenDialog( HTML_FileOpenDialog_t *p )
{
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->FileLoadDialogResponse( p->unBrowserHandle, NULL );
	}
}
void CHtmlSteamWebSurfaceGlobalCallbacksListener::OnHTMLStartRequest( HTML_StartRequest_t *p )
{
	// Check if there is a real browser object listening for this callback, if yes then let the real browser handle
	// if there's no browser then deny
	if ( m_rbKnownBrowserCallbackHandlers.Find( p->unBrowserHandle ) != m_rbKnownBrowserCallbackHandlers.InvalidIndex() )
		return;

	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->AllowStartRequest( p->unBrowserHandle, false );
	}
}
void CHtmlSteamWebSurfaceGlobalCallbacksListener::OnBrowserReady( HTML_BrowserReady_t *p )
{
	// Steamworks SDK doesn't really expect anything from our callback, having it here for compliance with MUST HAVE CALLBACKS
}
#endif



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHTML::CHTML( CPanel2D *parent, const char * pchPanelID, bool bPopup ) : CPanel2D( parent, pchPanelID ),
	m_scheduledProcessController( MAKE_SCHEDULED_FUNC( CHTML::ProcessSteamController ) )
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	, m_LinkAtPosRespose( this, &CHTML::OnLinkAtPositionResponse )
	, m_HTML_NeedsPaint( this, &CHTML::OnHTMLNeedsPaint )
	, m_HTML_StartRequest( this, &CHTML::OnHTMLStartRequest )
	, m_HTML_CloseBrowser( this, &CHTML::OnHTMLCloseBrowser )
	, m_HTML_URLChanged( this, &CHTML::OnHTMLURLChanged )
	, m_HTML_FinishedRequest( this, &CHTML::OnHTMLFinishedRequest )
	, m_HTML_OpenLinkInNewTab( this, &CHTML::OnHTMLOpenLinkInNewTab )
	, m_HTML_ChangedTitle( this, &CHTML::OnHTMLChangedTitle )
	, m_HTML_SearchResults( this, &CHTML::OnHTMLSearchResults )
	, m_HTML_CanGoBackAndForward( this, &CHTML::OnHTMLCanGoBackAndForward )
	, m_HTML_HorizontalScroll( this, &CHTML::OnHTMLHorizontalScroll )
	, m_HTML_VerticalScroll( this, &CHTML::OnHTMLVerticalScroll )
	, m_HTML_JSAlert( this, &CHTML::OnHTMLJSAlert )
	, m_HTML_JSConfirm( this, &CHTML::OnHTMLJSConfirm )
	, m_HTML_FileOpenDialog( this, &CHTML::OnHTMLFileOpenDialog )
	, m_HTML_NewWindow( this, &CHTML::OnHTMLNewWindow )
	, m_HTML_SetCursor( this, &CHTML::OnHTMLSetCursor )
	, m_HTML_StatusText( this, &CHTML::OnHTMLStatusText )
	, m_HTML_ShowToolTip( this, &CHTML::OnHTMLShowToolTip )
	, m_HTML_UpdateToolTip( this, &CHTML::OnHTMLUpdateToolTip )
	, m_HTML_HideToolTip( this, &CHTML::OnHTMLHideToolTip )
#endif
{
#if HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER
	if ( !g_pHtmlSteamWebSurfaceGlobalCallbacksListener )
		g_pHtmlSteamWebSurfaceGlobalCallbacksListener = new CHtmlSteamWebSurfaceGlobalCallbacksListener;
#endif

	m_nTextureSerialCombo = -1;
	m_nTextureSerial = -1;
	m_iBrowser = -1;
	m_PageLoadCount = 0;
	m_bLastKeyFocus = BHasKeyFocus();
	m_pVerticalScrollBar = NULL;
	m_pHorizontalScrollBar = NULL;
	m_bSuppressTextureLoads = false;
	m_bCaptureThumbNailThisFrame = false;
	m_bCommenceZoomOperationOnTextureUpload = false;
	m_nWindowWide = 0;
	m_nWindowTall = 0;
	m_bFullScreen = false;
	m_bPopupVisible = false;
	m_bConfigureYouTubeHTML5OptIn = false;
	m_bMousePanningActive = false;
	m_bIsSecure = m_bIsCertError = 	m_bIsEVCert = false;
	m_sCertName = "";
	m_bEmbedded = false;
	m_bIgnoreCursor = false;
	m_bFocusEventSentForClick = false;
	m_bDidMousePanWhileMouseDown = false;
	m_bWaitingForZoomResponse = false;
	m_pTooltip = NULL;
	m_bGotKeyDown = false;
	m_flMouseLastX = -1.0f;
	m_flMouseLastY = -1.0f;
	m_bReady = false;
	m_pPopupChild = NULL;
	m_flLastHorizontalScrollPos = m_flLastVeritcalScrollPos = 0;
	m_bIgnoreMouseBackForwardButtons = false;
	m_flInitialZoomLevel = 0.0f;
	m_bMarkZoomStart = false;
	m_bClickingLeftPad = false;
	m_bVerticalAxisSnap = false;
	m_bInvertScrolling = false;
	m_flZoomSwipeOriginPosition =  0.0f;
	m_flStartZoomTime = 0.0f;
	m_hActiveControllerHandle = 0;
	m_hCursorAnalogAction = 0;
	m_hScrollAnalogAction = 0;
	m_hZoomAnalogAction = 0;
	m_bControllerScrollThunked = false;
	m_bVRTouchPadFingerDown = false;
	m_flVRTouchLinearMoveDistanceForHaptics = 0.0f;

	m_bInitialized = true;
	m_bInFind = false;
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	m_bControlPageScrolling = false;
#else
	m_bControlPageScrolling = true;
#endif
	m_flZoom = 1.0f;
	m_nTextureWide = 0;
	m_nTextureTall = 0;

	m_flGamePadInputTime = 0.0f;

	m_flCursorX = 0.0f;
	m_flCursorY = 0.0f;

	// this panel wraps the actual html texture and renders it inside of the html control
	m_pTexurePanel = new CTexturePanel( this, "HTMLTexture" );
	AccessStyleDirty()->SetFlowChildren( k_EFlowNone ); // make sure we never flow our children


	m_pMousePanningImage = new CImagePanel( this, "MousePanningImage" );
	m_pMousePanningImage->SetImage( "file://{images}/browser/browser_mousepan.png" );

	if ( m_bLastKeyFocus )
	{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->SetKeyFocus( m_HTMLBrowser, m_bLastKeyFocus );
		}
#else
		CHTMLProtoBufMsg<CMsgSetFocus> cmd( eHTMLCommands_SetFocus );
		cmd.Body().set_focus( m_bLastKeyFocus );
		DISPATCH_MESSAGE( eHTMLCommands_SetFocus );
#endif
	}

	CFileResource fileResource( "file://{resources}/browser/webkit.css" );

	CUtlBuffer buf;
	UIEngine()->UIFileSystem()->LoadFileIntoBuffer( fileResource.GetReferencePath(), buf, true );

	CUtlStringBuilder strUserCSS;
	if ( buf.TellPut() )
	{
		strUserCSS.Append((const char*)buf.Base());
	}


	// copied from k_EAnimationEaseIn
	Vector2D vecPoints[4];
	panorama::GetAnimationCurveControlPoints( panorama::k_EAnimationEaseIn, vecPoints );
	m_ScrollBezier.SetControlPoints( vecPoints );
	m_MousePanBezier.SetControlPoints( vecPoints );

#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( !UIEngine()->AccessHTMLController()->BIsBrowserEnabled() )
	{
		m_pBrowserDisabledMessage = new CLabel( this, "BrowserDisabledMessage" );
		m_pBrowserDisabledMessage->SetText( "#Steam_Browser_Disabled" );
		AddClass( "BrowserDisabled" );
	}
	else 
#endif /* !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK ) */
	if ( !bPopup )
	{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface()  )
		{
#ifdef PANORAMA_USE_S1WRAPPER
			const char *pchUserAgent = "CSGO Client";
#else
			const char *pchUserAgent = "Source2 HTML";
#endif
			SteamAPICall_t hSteamAPICall = SteamHTMLSurface()->CreateBrowser( "CSGO Client", strUserCSS.String() );
			m_SteamCallResultBrowserReady.Set( hSteamAPICall, this, &CHTML::OnBrowserReady );
		}
#else
		const float dpiScaling = GetParentWindow()->GetSurfaceHeight() < 1080 ? 1.25f : 2.0f;
		bool bSuccess = UIEngine()->AccessHTMLController()->CreateBrowser( this, false, "Valve Steam Tenfoot", strUserCSS.String(), true /* support un-composited dropdowns */, dpiScaling /* dpi scaling */ );
		Assert( bSuccess );
#endif
	}

	RegisterEventHandler( GamepadInput(), this, &CHTML::OnGamepadInput );
	RegisterEventHandler( HTMLUpdatePageSize(), this, &CHTML::OnSetBrowserSize );
	RegisterEventHandler( HTMLFormFocusPending(), this, &CHTML::OnHTMLFormFocusPending );
	RegisterEventHandler( InputFocusSet(), this, &CHTML::OnInputFocusSet );
	RegisterEventHandler( InputFocusLost(), this, &CHTML::OnInputFocusLost );
	RegisterEventHandler( InputFocusTopLevelChanged(), this, &CHTML::OnInputFocusTopLevelChanged );
	RegisterEventHandler( HTMLScreenShotCaptured(), this, &CHTML::OnHTMLScreenShotCaptured );
	RegisterEventHandler( HTMLCommitZoom(), this, &CHTML::OnHTMLCommitZoom );
	RegisterEventHandler( HTMLRequestRepaint(), this, &CHTML::OnHTMLRequestRepaint );
	
#if defined( SOURCE2_PANORAMA ) 
	RegisterForUnhandledEvent( FileOpenDialogFilesSelected(), this, &CHTML::OnFileOpenDialogFilesSelected );
#endif
	
	m_bSteamControllerEnabled = false;
	m_bHasControllerFocus = false;
	
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	EnableSteamControllerSupport();
#endif
	
}


//-----------------------------------------------------------------------------
// Purpose: Call on a HTML panel to start processing Steam controller
// Your app must have a valid Steam controller configuration loaded with the right
// action sets and actions embedded into it
//-----------------------------------------------------------------------------
void CHTML::EnableSteamControllerSupport()
{
	if ( m_bSteamControllerEnabled )
	{
		return;
	}
	
	m_bSteamControllerEnabled = true;
	
	InitSteamController();
	
	UpdateCursorBehaviourOnFocusChange( m_bHasControllerFocus );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHTML::~CHTML()
{
#if defined( SOURCE2_PANORAMA )
	UnregisterForUnhandledEvent( FileOpenDialogFilesSelected(), this, &CHTML::OnFileOpenDialogFilesSelected );
#endif

	Shutdown();
}


#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: SteamAPI way to get a constructed browser
//-----------------------------------------------------------------------------
void CHTML::OnBrowserReady( HTML_BrowserReady_t *pBrowserReady, bool bIOFailure )
{
	if ( bIOFailure )
		return;

	m_bReady = true;
	m_HTMLBrowser = pBrowserReady->unBrowserHandle;

#if HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER
	if ( g_pHtmlSteamWebSurfaceGlobalCallbacksListener )
	{
		g_pHtmlSteamWebSurfaceGlobalCallbacksListener->m_rbKnownBrowserCallbackHandlers.Insert( m_HTMLBrowser );
	}
#endif

	if ( !m_sURLToLoad.IsEmpty() )
	{
		if ( m_sURLPostData.IsEmpty() )
			OpenURL( m_sURLToLoad.String() );
		else
			PostURL( m_sURLToLoad.String(), m_sURLPostData.String() );
	}

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( m_scrollHorizontal.m_nPageSize, m_scrollVertical.m_nPageSize, false, flLeft, flTop, flRight, flBottom );

	m_nHTMLPageWide = m_nHTMLPageTall = 0;

	// make it fill the control minus any padding we have
	SetBrowserSize( GetActualLayoutWidth() - flLeft - flRight, GetActualLayoutHeight() - flTop - flBottom );
	m_ScrollLeft.Reset();
	m_ScrollUp.Reset();
}

#endif


//-----------------------------------------------------------------------------
// Purpose: snag the url to use once the browser is created
//-----------------------------------------------------------------------------
bool CHTML::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol imgSymbol( "url" );
	static CPanoramaSymbol embeddedSymbol( "embedded" );
	static CPanoramaSymbol htmlScrollingSymbol( "manualscroll" );
	static CPanoramaSymbol dpiAwareSymbol( "dpiaware" );
	if ( symName == imgSymbol )
	{
		m_sURLToLoad = pchValue;
		return true;
	}
	else if ( symName == embeddedSymbol )
	{
		bool bEmbedded = false;
		DbgVerify( CSSHelpers::BParseTrueFalse( pchValue, &bEmbedded ) );
		SetEmbeddedMode( bEmbedded );
		return true;
	}
	else if ( symName == htmlScrollingSymbol )
	{
		bool bManualScroll = false;
		DbgVerify( CSSHelpers::BParseTrueFalse( pchValue, &bManualScroll ) );
		SetManualHTMLScroll( bManualScroll );
		return true;
	}
	else if ( symName == dpiAwareSymbol )
	{
		bool bClientAwareDPI = false;
		DbgVerify( CSSHelpers::BParseTrueFalse( pchValue, &bClientAwareDPI ) );
		const float dpiScaling = GetParentWindow()->GetSurfaceHeight() < 1080 ? 1.25f : 2.0f;
		SetPageDPI( bClientAwareDPI ? 1.0f : dpiScaling );
		return true;
	}
	else
	{
		return BaseClass::BSetProperty( symName, pchValue );
	}
}


//-----------------------------------------------------------------------------
// Purpose: set the DPI to sepot for this page
//-----------------------------------------------------------------------------
void CHTML::SetPageDPI( float flDPI )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	AssertMsg( false, "Needs impl" );
#else
	CHTMLProtoBufMsg<CMsgScreenDPI> cmd( eHTMLCommands_SetScreenDPI );
	cmd.Body().set_dpi_scaling( flDPI );
	DISPATCH_MESSAGE( eHTMLCommands_SetScreenDPI );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: set if this control is being embedded in another page, used to enable RS scrolling
//-----------------------------------------------------------------------------
void CHTML::SetEmbeddedMode( bool bState )
{
	m_bEmbedded = bState;
	m_pMousePanningImage->SetVisible( !m_bEmbedded );
}


//-----------------------------------------------------------------------------
// Purpose: helper to make our scrollbar panel
//-----------------------------------------------------------------------------
CScrollBar *CHTML::MakeScrollBar( bool bHorizontal )
{
	CScrollBar *pScrollBar = NULL;

	if ( bHorizontal )
	{
		UIPanel()->SetInScrollbarConstruction( true );
		m_pHorizontalScrollBar = new CHTMLHorizontalScrollBar( this, "HorizontalScrollBar" );
		UIPanel()->SetInScrollbarConstruction( false );
		pScrollBar = m_pHorizontalScrollBar;
	}
	else
	{
		UIPanel()->SetInScrollbarConstruction( true );
		m_pVerticalScrollBar = new CHTMLVerticalScrollBar( this, "VerticalScrollBar" );
		UIPanel()->SetInScrollbarConstruction( false );
		pScrollBar = m_pVerticalScrollBar;
	}

	float flZindex = 0.0f;
	m_pTexurePanel->AccessStyle()->GetZIndex( flZindex );
	pScrollBar->AccessStyle()->SetZIndex( flZindex + 1 ); // manually pull the scrollbar forward on the texture panel

	return pScrollBar;
}


//-----------------------------------------------------------------------------
// Purpose: given cef and window sizes decide if and how to draw a scroll bar
//-----------------------------------------------------------------------------
bool CHTML::SetupScrollBar( bool bHorizontal, float flMaxSize )
{
	bool bCreatedScrollBar = false;
	CScrollBar *pScrollBar = NULL;

	if ( m_bControlPageScrolling )
	{
		const ScrollData_t &scrollData = bHorizontal ? m_scrollHorizontal : m_scrollVertical;
		const float flContentSize = bHorizontal ? GetContentWidth() : GetContentHeight();
		
		if( (scrollData.m_bVisible && scrollData.m_nMaxScroll > 0) || scrollData.m_nPageSize > flContentSize )
		{
			if( bHorizontal )
			{
				pScrollBar = m_pHorizontalScrollBar;
			}
			else
			{
				pScrollBar = m_pVerticalScrollBar;
			}

			// Need scroll
			if( !pScrollBar )
			{
				bCreatedScrollBar = true;
				pScrollBar = MakeScrollBar( bHorizontal );
				pScrollBar->SetScrollWindowPosition( scrollData.m_nScroll, true );
			}

			pScrollBar->SetVisible( true );
			pScrollBar->SetRangeMinMax( 0, scrollData.m_nPageSize - flMaxSize / 2 );
			pScrollBar->SetScrollWindowSize( flMaxSize / 2 );
		}
	}

	if( !pScrollBar )
	{
		// we shouldn't have a scrollbar
		if( bHorizontal )
		{
			SAFE_DELETE( m_pHorizontalScrollBar );
		}
		else
		{
			SAFE_DELETE( m_pVerticalScrollBar );
		}
	}

	return bCreatedScrollBar;
}


//-----------------------------------------------------------------------------
// Purpose: let the web browser know how big to be
//-----------------------------------------------------------------------------
bool CHTML::OnSetBrowserSize( const CPanelPtr< IUIPanel > &pPanel, int nWide, int nTall )
{
	SetBrowserSize( nWide, nTall );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: let the web browser know how big to be
//-----------------------------------------------------------------------------
void CHTML::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	// update chrome with our new size
	{
		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( m_scrollHorizontal.m_nPageSize, m_scrollVertical.m_nPageSize, false, flLeft, flTop, flRight, flBottom );

		// be as wide as the screen minus any padding we have, delay it until we are out of the layout loop so any size changes we make apply next time around
		DispatchEventAsync( 0.0f, panorama::HTMLUpdatePageSize(), this, flFinalWidth - flLeft - flRight, flFinalHeight - flTop - flBottom );
	}

	if ( m_pDoubleBufferedTexture ) // only do scroll bars if we are going to draw a texture
	{
		 SetupScrollBar( true, flFinalWidth );
		 SetupScrollBar( false, flFinalHeight );
	}

	// Now layout the scroll bars, since we've set both their values, we use finalHeight/finalWidth
	// rather than containerWidth/containerHeight here since scrollbars get to ignore parents padding values!
	if ( m_pVerticalScrollBar )
	{
		CUILength marginLeft, marginTop, marginRight, marginBottom;
		m_pVerticalScrollBar->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flFinalWidth );
		marginRight.ConvertToLength( flFinalWidth );
		marginTop.ConvertToLength( flFinalHeight );
		marginBottom.ConvertToLength( flFinalHeight );

		m_pVerticalScrollBar->DesiredLayoutSizeTraverse( flFinalWidth, flFinalHeight );
		m_pVerticalScrollBar->LayoutTraverse( 0.0f, 0.0f, flFinalWidth - marginLeft.GetValue() - marginRight.GetValue(), flFinalHeight - marginTop.GetValue() - marginBottom.GetValue() );

		if ( m_pVerticalScrollBar->GetScrollWindowPosition() != m_flLastVeritcalScrollPos )
		{
			m_flLastVeritcalScrollPos = m_pVerticalScrollBar->GetScrollWindowPosition();
			SetVerticalScroll( m_pVerticalScrollBar->GetScrollWindowPosition() );
			m_pVerticalScrollBar->Normalize();
		}
	}

	if ( m_pHorizontalScrollBar )
	{
		CUILength marginLeft, marginTop, marginRight, marginBottom;
		m_pHorizontalScrollBar->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flFinalWidth );
		marginRight.ConvertToLength( flFinalWidth );
		marginTop.ConvertToLength( flFinalHeight );
		marginBottom.ConvertToLength( flFinalHeight );

		m_pHorizontalScrollBar->DesiredLayoutSizeTraverse( flFinalWidth, flFinalHeight );
		m_pHorizontalScrollBar->LayoutTraverse( 0.0f, 0.0f, flFinalWidth - marginLeft.GetValue() - marginRight.GetValue(), flFinalHeight - marginTop.GetValue() - marginBottom.GetValue() );

		if ( m_pHorizontalScrollBar->GetScrollWindowPosition() != m_flLastHorizontalScrollPos )
		{
			m_flLastHorizontalScrollPos = m_pHorizontalScrollBar->GetScrollWindowPosition();
			SetHorizontalScroll( m_pHorizontalScrollBar->GetScrollWindowPosition() );
			m_pHorizontalScrollBar->Normalize();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: paint the html texture and also ack any rgba uploads from the texture
//-----------------------------------------------------------------------------
void CHTML::Paint()
{
	VPROF_BUDGET("CHTML::Paint", VPROF_BUDGETGROUP_TENFOOT );

	sm_PaintCount++;

	BaseClass::Paint();

	if ( m_pVerticalScrollBar && m_pVerticalScrollBar->BIsVisible() )
		m_pVerticalScrollBar->PaintTraverse();
	if ( m_pHorizontalScrollBar && m_pHorizontalScrollBar->BIsVisible() )
		m_pHorizontalScrollBar->PaintTraverse();
}


//-----------------------------------------------------------------------------
// Purpose: tell the html control when it gets key focus
//-----------------------------------------------------------------------------
void CHTML::OnStylesChanged()
{
	BaseClass::OnStylesChanged();
	if ( m_bLastKeyFocus != BHasKeyFocus() )
	{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->SetKeyFocus( m_HTMLBrowser, BHasKeyFocus() );
		}
#else
		CHTMLProtoBufMsg<CMsgSetFocus> cmd( eHTMLCommands_SetFocus );
		cmd.Body().set_focus( BHasKeyFocus() );
		DISPATCH_MESSAGE( eHTMLCommands_SetFocus );
		m_bLastKeyFocus = BHasKeyFocus();
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel is measured
//-----------------------------------------------------------------------------
void CHTML::OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	BaseClass::OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );

	if ( m_scrollHorizontal.m_nPageSize && m_scrollVertical.m_nPageSize )
	{
		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( m_scrollHorizontal.m_nPageSize * GetActualUIScaleX(), m_scrollVertical.m_nPageSize * GetActualUIScaleY(), bFinalDimensions, flLeft, flTop, flRight, flBottom );

		*pflContentWidth = MAX( m_scrollHorizontal.m_nPageSize * GetActualUIScaleX() + flLeft + flRight, *pflContentWidth );
		*pflContentHeight = MAX( m_scrollVertical.m_nPageSize *  GetActualUIScaleY() + flTop + flBottom, *pflContentHeight );
	}
}


//-----------------------------------------------------------------------------
// Purpose: shutdown this html object and remove ourselves from the map
//-----------------------------------------------------------------------------
void CHTML::Shutdown()
{
	if ( !m_bInitialized )
		return;

#if HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER
	if ( g_pHtmlSteamWebSurfaceGlobalCallbacksListener )
	{
		g_pHtmlSteamWebSurfaceGlobalCallbacksListener->m_rbKnownBrowserCallbackHandlers.Remove( m_HTMLBrowser );
	}
#endif

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->RemoveBrowser( m_HTMLBrowser );
	}
#endif

	if ( m_iBrowser >= 0 )
	{
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
		CHTMLProtoBufMsg<CMsgBrowserRemove> cmd( eHTMLCommands_BrowserRemove );
		DISPATCH_MESSAGE( eHTMLCommands_BrowserRemove );
#endif
	}

#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	UIEngine()->AccessHTMLController()->RemoveBrowser( this );
#endif

	ReleaseTextureMemory();

	UpdateCursorBehaviourOnFocusChange( false );

	m_bInitialized = false;
}


//-----------------------------------------------------------------------------
// Purpose: open this simple url, doesn't post any extra data
//-----------------------------------------------------------------------------
void CHTML::OpenURL(const char *pchURL)
{
	if ( !pchURL )
	{
		pchURL = "about:blank";
	}

	if ( pchURL && pchURL[0] )
	{
		if ( !m_bReady )
		{
			m_sURLToLoad = pchURL;
		}
		else
		{
			for ( int i = 0; i < UIEngine()->GetXHeaderCount(); i++ )
			{
				CUtlString strName;
				CUtlString strValue;
				UIEngine()->GetXHeader( i, strName, strValue );
				AddHeader( strName, strValue );
			}

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
			if ( SteamHTMLSurface() )
			{
				SteamHTMLSurface()->LoadURL( m_HTMLBrowser, pchURL, NULL );
			}
#else
			CHTMLProtoBufMsg<CMsgPostURL> cmd( eHTMLCommands_PostURL );
			cmd.Body().set_url( pchURL );
			cmd.Body().set_pageserial( ++m_PageLoadCount );
			DISPATCH_MESSAGE( eHTMLCommands_PostURL );
#endif
			m_sCurrentURL = pchURL;
			ReleaseTextureMemory();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: stop loading the current page
//-----------------------------------------------------------------------------
void CHTML::StopLoading()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->StopLoad( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgStopLoad> cmd( eHTMLCommands_StopLoad );
	DISPATCH_MESSAGE( eHTMLCommands_StopLoad );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: reload the current page
//-----------------------------------------------------------------------------
void CHTML::Refresh()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->Reload( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgReload> cmd( eHTMLCommands_Reload );
	DISPATCH_MESSAGE( eHTMLCommands_Reload );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: go back to the last visited url (if there is one)
//-----------------------------------------------------------------------------
void CHTML::GoBack()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->GoBack( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgGoBack> cmd( eHTMLCommands_GoBack );
	DISPATCH_MESSAGE( eHTMLCommands_GoBack );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: go forward in the history stack if there is an item
//-----------------------------------------------------------------------------
void CHTML::GoForward()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->GoForward( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgGoForward> cmd( eHTMLCommands_GoForward );
	DISPATCH_MESSAGE( eHTMLCommands_GoForward );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHTML::BCanGoBack()
{
	return m_bCanGoBack;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHTML::BCanGoForward()
{
	return m_bCanGoForward;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if we aren't squelching mouse input right now
//			due to either a zoom or gamepad being used
//-----------------------------------------------------------------------------
bool CHTML::BAcceptMouseInput()
{
	if ( !m_flGamePadInputTime )
		return true; // always allow mouse input if gamepad was never used

	return ( m_flGamePadInputTime + k_flIgnoreMouseAfterGamepadInputTimer ) < Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Purpose: Pause all audio/video elements and throttle updates to 1hz. CPU use will drop to effectively 0.
//			It does not free memory, only navigating away from the page can do that.
//-----------------------------------------------------------------------------
void CHTML::SetBackgroundMode( bool bBackgroundMode )
{
#if defined( SOURCE2_PANORAMA )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetBackgroundMode( m_HTMLBrowser, bBackgroundMode );
	}
#else
	Assert( !"Not implemented/supported" );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: helper to convert UI modifiers to CEF ones
//-----------------------------------------------------------------------------
int GetKeyModifiers( int nUIModifiers )
{
	int nModifierCodes = 0;
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( nUIModifiers & MODIFIER_LCONTROL || nUIModifiers & MODIFIER_RCONTROL )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_CtrlDown;

	if ( nUIModifiers & MODIFIER_LALT || nUIModifiers & MODIFIER_RALT )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_AltDown;

	if ( nUIModifiers & MODIFIER_LSHIFT || nUIModifiers & MODIFIER_RSHIFT )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_ShiftDown;

#ifdef OSX
	// for now pipe through the cmd-key to be like the control key so we get copy/paste
	if ( nUIModifiers & MODIFIER_LWIN || nUIModifiers & MODIFIER_RWIN )
		nModifierCodes |= ISteamHTMLSurface::k_eHTMLKeyModifier_ShiftDown;
#endif
#else
	if ( nUIModifiers & MODIFIER_LCONTROL || nUIModifiers & MODIFIER_RCONTROL )
		nModifierCodes |= IInputEventHTML::CrtlDown;

	if ( nUIModifiers & MODIFIER_LALT || nUIModifiers & MODIFIER_RALT )
		nModifierCodes |= IInputEventHTML::AltDown;

	if ( nUIModifiers & MODIFIER_LSHIFT || nUIModifiers & MODIFIER_RSHIFT )
		nModifierCodes |= IInputEventHTML::ShiftDown;

#ifdef OSX
	// for now pipe through the cmd-key to be like the control key so we get copy/paste
	if ( nUIModifiers & MODIFIER_LWIN || nUIModifiers & MODIFIER_RWIN )
		nModifierCodes |= IInputEventHTML::CrtlDown;
#endif
#endif

	return nModifierCodes;
}


//-----------------------------------------------------------------------------
// Purpose: mouse pressed
//-----------------------------------------------------------------------------
bool CHTML::OnMouseButtonDown( const MouseData_t &code )
{
	if ( BAcceptMouseInput() && (BHasDescendantKeyFocus() || BHasKeyFocus() || GetMouseCanActivate() == k_EMouseCanActivateUnfocused ) )
	{
		bool bWasMousePanningActive = m_bMousePanningActive;
		if ( m_bMousePanningActive )
		{
			m_bMousePanningActive = false;
			RemoveClass( "MousePanning" );
			DispatchEvent( HTMLStopMousePanning(), this );
		}

		if ( code.m_MouseCode == MOUSE_MIDDLE && !m_LinkAtPos.m_bLiveLink )
		{
			if ( !bWasMousePanningActive )
			{
				m_bDidMousePanWhileMouseDown = false;
				m_bMousePanningActive = true;
				m_vecMousePanningPos.x = m_flCursorX;
				m_vecMousePanningPos.y = m_flCursorY;
				float flMouseImageSize = 50.0f;
				const char *pszValue = m_pMousePanningImage->GetLayoutFileDefine( "mousepanningcursorsize" );
				if ( pszValue )
					flMouseImageSize = V_atof( pszValue );

				CUILength mouseX( m_vecMousePanningPos.x - flMouseImageSize/2, CUILength::k_EUILengthLength );
				CUILength mouseY( m_vecMousePanningPos.y - flMouseImageSize/2, CUILength::k_EUILengthLength );

				mouseX.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
				mouseY.ScaleLengthValue( 1.0f / GetActualUIScaleY() );

				m_pMousePanningImage->SetPositionWithoutTransition( mouseX, mouseY, CUILength( 0.0f, CUILength::k_EUILengthLength ) );
				AddClass( "MousePanning" );

				DispatchEvent( HTMLStartMousePanning(), this, m_vecMousePanningPos.x, m_vecMousePanningPos.y );
			}
		}
		else if ( BIgnoreMouseBackForwardButtons() && ( code.m_MouseCode == MOUSE_4 || code.m_MouseCode == MOUSE_5 ) )
		{
			return false; // bubble
		}
		else if ( code.m_MouseCode == MOUSE_4 )
		{
			GoBack();
		}
		else if ( code.m_MouseCode == MOUSE_5 )
		{
			GoForward();
		}
		else
		{
			float flNow = Plat_FloatTime();
#if !defined(NO_STEAM)
			float flSteamControllerUsage = UIInputEngine()->GetLastSteamControllerActiveTime();
#else
			float flSteamControllerUsage = UIInputEngine()->GetLastGamePadControllerActiveTime();
#endif
			if ( m_LinkAtPos.m_bInput && m_evtFocus.m_bInput && ( flNow - flSteamControllerUsage  ) < 1.0f )
			{
				m_bFocusEventSentForClick = false;
				DispatchEventAsync( k_flFormFocusDelayCheck, HTMLFormFocusPending(), this ); // queue a call to check in 1 sec that we sent a form focus event
			}

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
			if ( SteamHTMLSurface() )
			{
				SteamHTMLSurface()->MouseDown( m_HTMLBrowser, (ISteamHTMLSurface::EHTMLMouseButton)code.m_MouseCode );
			}
#else
			CHTMLProtoBufMsg<CMsgMouseDown> cmd( eHTMLCommands_MouseDown );
			cmd.Body().set_mouse_button( code.m_MouseCode );
			cmd.Body().set_modifiers( GetKeyModifiers( code.m_Modifiers ) );
			DISPATCH_MESSAGE( eHTMLCommands_MouseDown );
#endif
			m_evtFocus.m_bUserInputThisPage = true;
		}
		if ( BHasDescendantKeyFocus() || BHasKeyFocus() )
		{
			return code.m_MouseCode != MOUSE_RIGHT; // only let right click bubble
		}
		else
		{
			Assert( GetMouseCanActivate() == k_EMouseCanActivateUnfocused );
			return false;
		}
	}
	return false; // bubble to our parent also
}


//-----------------------------------------------------------------------------
// Purpose: mouse released
//-----------------------------------------------------------------------------
bool CHTML::OnMouseButtonUp( const MouseData_t &code )
{
	if ( BAcceptMouseInput() )
	{
		if ( code.m_MouseCode == MOUSE_MIDDLE && m_bMousePanningActive && m_bDidMousePanWhileMouseDown )
		{
			m_bMousePanningActive = false;
			RemoveClass( "MousePanning" );
			DispatchEvent( HTMLStopMousePanning(), this );
		}
		else
		{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
			if ( SteamHTMLSurface() )
			{
				SteamHTMLSurface()->MouseUp( m_HTMLBrowser, (ISteamHTMLSurface::EHTMLMouseButton)code.m_MouseCode );
			}
#else
			CHTMLProtoBufMsg<CMsgMouseUp> cmd( eHTMLCommands_MouseUp );
			cmd.Body().set_mouse_button( code.m_MouseCode );
			cmd.Body().set_modifiers( GetKeyModifiers( code.m_Modifiers ) );
			DISPATCH_MESSAGE( eHTMLCommands_MouseUp );
#endif
		}
		m_bDidMousePanWhileMouseDown = false;
		return code.m_MouseCode != MOUSE_RIGHT; // only let right click bubble
	}
	return false; // bubble to our parent also
}


//-----------------------------------------------------------------------------
// Purpose: mouse double clicked
//-----------------------------------------------------------------------------
bool CHTML::OnMouseButtonDoubleClick( const MouseData_t &code )
{
	if ( BAcceptMouseInput() )
	{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->MouseDoubleClick( m_HTMLBrowser, (ISteamHTMLSurface::EHTMLMouseButton)code.m_MouseCode );
		}
#else
		CHTMLProtoBufMsg<CMsgMouseDblClick> cmd( eHTMLCommands_MouseDblClick );
		cmd.Body().set_mouse_button( code.m_MouseCode );
		cmd.Body().set_modifiers( GetKeyModifiers( code.m_Modifiers ) );
		DISPATCH_MESSAGE( eHTMLCommands_MouseDblClick );
#endif
	}
	return false; // bubble to our parent also
}




//-----------------------------------------------------------------------------
// Purpose: Stateless helper func for converting client window coordinates to surface coordinates.
//-----------------------------------------------------------------------------
void ConvertSurfaceCoordinatesToClient( float *px, float *py, uint32 unWindowWidth, uint32 unWindowHeight, uint32 unSurfaceWidth, uint32 unSurfaceHeight )
{
	float x = *px / (float) unSurfaceWidth;
	float y = *py / (float) unSurfaceHeight;

	*px = x * (float) unWindowWidth;
	*py = y * (float) unWindowHeight;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHTML::InitSteamController()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamController() )
	{
		SteamController()->Init();
		m_hCursorAnalogAction = SteamController()->GetAnalogActionHandle( "Cursor" );
		m_hScrollAnalogAction = SteamController()->GetAnalogActionHandle( "Scroll" );
		m_hZoomAnalogAction = SteamController()->GetAnalogActionHandle( "Zoom" );
	}
#else
	ClientControllerLocal()->Init( false );
	m_hCursorAnalogAction = ClientController()->GetAnalogActionHandle( k_nGameIDControllerConfigs_BigPicture, "Cursor" );
	m_hScrollAnalogAction = ClientController()->GetAnalogActionHandle( k_nGameIDControllerConfigs_BigPicture, "Scroll" );
	m_hZoomAnalogAction = ClientController()->GetAnalogActionHandle( k_nGameIDControllerConfigs_BigPicture, "Zoom" );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHTML::SetControllerActionSet( const char *pszActionSet )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamController() )
	{
		ControllerActionSetHandle_t hActionSet = SteamController()->GetActionSetHandle( pszActionSet );
		SteamController()->ActivateActionSet( STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, hActionSet );
	}
#else
	ControllerActionSetHandle_t hActionSet = ClientController()->GetActionSetHandle( k_nGameIDControllerConfigs_BigPicture, pszActionSet );
	ClientController()->ActivateActionSet( k_nGameIDControllerConfigs_BigPicture, STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, hActionSet );
#endif	
}

static inline ControllerAnalogActionData_t GetAnalogActionData( ControllerHandle_t ulControllerHandle, ControllerAnalogActionHandle_t AnalogActionHandle )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	return SteamController()->GetAnalogActionData( ulControllerHandle, AnalogActionHandle );
#else
	return ClientControllerLocal()->GetAnalogActionData( k_nGameIDControllerConfigs_BigPicture, ulControllerHandle, AnalogActionHandle );
#endif
}

static inline void StopAnalogActionMomentum( ControllerHandle_t ulControllerHandle, ControllerAnalogActionHandle_t AnalogActionHandle )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	SteamController()->StopAnalogActionMomentum( ulControllerHandle, AnalogActionHandle );
#else
	ClientController()->StopAnalogActionMomentum( ulControllerHandle, AnalogActionHandle );
#endif
}

static inline void TriggerRepeatedHapticPulse( ControllerHandle_t controllerHandle, ESteamControllerPad eTargetPad, unsigned short usDurationMicroSec, unsigned short usOffMicroSec, unsigned short unRepeat, unsigned int nFlags )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	// Uncomment as soon as we release a new Steamworks SDK
// 	SteamController()->TriggerRepeatedHapticPulse( controllerHandle, eTargetPad, usDurationMicroSec, usOffMicroSec, unRepeat, nFlags );
#else
	ClientControllerLocal()->TriggerRepeatedHapticPulseOnHandle( controllerHandle, eTargetPad, usDurationMicroSec, usOffMicroSec, unRepeat, nFlags );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHTML::ProcessSteamController()
{
	static const int k_nVerticalAxisSnapThreshold = 25;

	// Use the first available steam controller for all interaction. We can call this each frame to handle
	// a controller disconnecting and a different one reconnecting. Handles are guaranteed to be unique for
	// a given controller, even across power cycles.

	m_scheduledProcessController.Schedule( 0 );

	if( m_hActiveControllerHandle == 0 )
		return;

	DbgAssert( BHasKeyFocus() );
	
	if ( !ClientControllerLocal() )
		return;

	ClientControllerLocal()->RunFrame();

	// Returns the current state of these supplied analog game action
	

	ControllerAnalogActionData_t scrollData = GetAnalogActionData( m_hActiveControllerHandle, m_hScrollAnalogAction );
	if( scrollData.bActive )
	{
		// Plat_OutputDebugString( "scrollData eMode %u, x %.1f, y %.1f \n", scrollData.eMode, scrollData.x, scrollData.y );

		if( m_bInvertScrolling )
		{
			scrollData.x *= -1.0f;
			scrollData.y *= -1.0f;
		}

		if( m_bVerticalAxisSnap )
		{
			if( fabs( scrollData.x ) > k_nVerticalAxisSnapThreshold )
			{
				m_bVerticalAxisSnap = false;
			}
			else
			{
				scrollData.x = 0.0f;
			}
		}
		
		bool bHitEdge = false;

		ScrollHelper( true, scrollData.x, 0.0f, false, &bHitEdge );
		ScrollHelper( false, scrollData.y, 0.0f, false, &bHitEdge );
		
		if ( bHitEdge && m_bControllerScrollThunked == false )
		{
			StopAnalogActionMomentum( m_hActiveControllerHandle, m_hScrollAnalogAction );
			
			TriggerRepeatedHapticPulse( m_hActiveControllerHandle, k_ESteamControllerPad_Left, 1500, 1500, 3, 0 );
			
			m_bControllerScrollThunked = true;
		}
		else if ( !bHitEdge )
		{
			m_bControllerScrollThunked = false;
		}
	}

	ControllerAnalogActionData_t zoomData = GetAnalogActionData( m_hActiveControllerHandle, m_hZoomAnalogAction );
	if( zoomData.bActive )
	{
		// Plat_OutputDebugString( "zoomData eMode %u, x %.1f, y %.1f \n", zoomData.eMode, zoomData.x, zoomData.y );
		IncrementPageScale( zoomData.x / 1000, false );
	}


	if( !m_bIgnoreCursor )
	{
		ControllerAnalogActionData_t cursorData = GetAnalogActionData( m_hActiveControllerHandle, m_hCursorAnalogAction );

		if( cursorData.bActive )
		{
			// Plat_OutputDebugString( "cursorData eMode %u, x %.1f, y %.1f\n", cursorData.eMode, cursorData.x, cursorData.y );
			IUIWindowInput *pInputWindow = GetParentWindow()->UIWindowInput();

			float flMouseX, flMouseY;
			pInputWindow->GetSurfaceMousePosition( flMouseX, flMouseY );

			flMouseX += cursorData.x;
			flMouseY += cursorData.y;

			// clamp to HTML panel
			float flPanelX, flPanelY;
			GetPositionWithinWindow( &flPanelX, &flPanelY );

			flMouseX = clamp( flMouseX, flPanelX, flPanelX + GetActualLayoutWidth() );
			flMouseY = clamp( flMouseY, flPanelY, flPanelY + GetActualLayoutHeight() );

			pInputWindow->OnMouseMoveSurfaceCoords(flMouseX, flMouseY);
		}
	}
}

#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
void CHTML::OnControllerConnected( ControllerConnected_t *pCallback )
{
	UpdateCursorBehaviourOnFocusChange( m_bHasControllerFocus );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHTML::UpdateCursorBehaviourOnFocusChange( bool bGotFocus )
{
	m_bHasControllerFocus = bGotFocus;
	
	if ( m_bSteamControllerEnabled == false )
	{
		return;
	}
	
	if( bGotFocus )
	{
		if ( ClientControllerLocal() )
		{
			ClientControllerLocal()->RunFrame();

			m_hActiveControllerHandle = 0;

			// If there's an active controller, and if we're not already using it, select the first one.
			ControllerHandle_t pHandles[ STEAM_CONTROLLER_MAX_COUNT ];
			if ( ClientControllerLocal()->GetConnectedControllers( pHandles ) > 0 )
			{
				m_hActiveControllerHandle = pHandles[ 0 ];
			}
			else
			{
				m_hActiveControllerHandle = 0; // off
			}

			// Plat_OutputDebugString( "UpdateCursorBehaviourOnFocusChange: %s\n", BHasSteamController() ? "has controller" : "no controller" );

			if ( BHasSteamController() )
			{
				SetControllerActionSet( "WebBrowser" );
			
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
				m_bInvertScrolling = ClientConfigStore()->GetBool( k_EConfigStoreUserLocal, "WebBrowserEnableAnalogAustralianScrolling", true );
#endif

				if ( !m_bIgnoreCursor )
				{
					// don't hide mouse cursor when user scrolls with steampad
					GetParentWindow()->EnableControllerCursor( true );
				}

				m_scheduledProcessController.Schedule( 0 );
			}
		}
	}
	else
	{
		if ( m_hActiveControllerHandle != 0 )
		{
			// back to default
			// Plat_OutputDebugString( "UpdateCursorBehaviourOnFocusChange: off \n" );

			SetControllerActionSet( "Default" );
			
			if( BHasSteamController() )
			{
				if( !m_bIgnoreCursor )
				{
					GetParentWindow()->EnableControllerCursor( false );
				}
			}

			m_hActiveControllerHandle = 0;
		}

		m_scheduledProcessController.Cancel();
	}
}

//-----------------------------------------------------------------------------
// Purpose: if web browser has focus, don't hide mouse cursor if controller is used
//-----------------------------------------------------------------------------
bool CHTML::OnInputFocusTopLevelChanged( panorama::CPanelPtr< panorama::IUIPanel > ptrPanel )
{
	CPanel2D *pPanel = ToPanel2D( ptrPanel.Get() );
	bool bGotFocus = ( pPanel == this ) || IsDescendantOf( pPanel );

	UpdateCursorBehaviourOnFocusChange( bGotFocus );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: the mouse wheel moved, let cef scroll
//-----------------------------------------------------------------------------
bool CHTML::OnMouseWheel( const MouseData_t &code )
{
	if ( BAcceptMouseInput() )
	{
		if ( !m_bControlPageScrolling )
		{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
			if ( SteamHTMLSurface() )
			{
				SteamHTMLSurface()->MouseWheel( m_HTMLBrowser, code.m_Delta * 10 );
			}
#else
			// Let CEF handle mouse scrolling and push new scroll values back to us
			CHTMLProtoBufMsg<CMsgMouseWheel> cmd( eHTMLCommands_MouseWheel );
			cmd.Body().set_delta( code.m_Delta );
			cmd.Body().set_modifiers( GetKeyModifiers( code.m_Modifiers ) );
			DISPATCH_MESSAGE( eHTMLCommands_MouseWheel );
#endif
		}
		else
		{
			if ( IsControlPressed( code.m_Modifiers ) )
			{
				IncrementPageScale( k_flMouseWheelZoomSpeedModifier * code.m_Delta ); // zoom rather than scroll when ctrl is pressed
			}
			else
			{
				ScrollPageUp( code.m_Delta * 50 );
			}
		}
	}
	return false; // bubble to our parent also
}


//-----------------------------------------------------------------------------
// Purpose: scroll event from the VR controller, do pad style scrolling
//-----------------------------------------------------------------------------
bool CHTML::OnVRTouchPad( const VRTouchEvent_t &code )
{
#ifndef SOURCE2_PANORAMA
	if ( !code.m_bPrimaryController )
		return true;

	if ( !m_bVRTouchPadFingerDown )
	{
		if ( code.m_bFingerDown )
		{
			m_bVRTouchPadFingerDown = true;
			m_flVRTouchLinearMoveDistanceForHaptics = 0.0f;
			m_vecVRTouchEvents.RemoveAll();
			return true; // ignore the first sample after a down touch
		}
	}

	if ( !code.m_bFingerDown )
	{
		// finger went up this frame, re-arm our bool so we can reset our movement when the finger next goes down
		m_bVRTouchPadFingerDown = false;
	}

	// filter out samples from right as the finger goes down as they are noisy
	if ( code.m_flFingerDown < 0.05f )
	{
		return true;
	}

	if ( fabs( code.m_fValueXRaw ) > 0.85 || fabs( code.m_fValueYRaw ) > 0.85 )
	{
		// ignore samples from the edge of the pad, they can be noisy
		return true;
	}

	m_vecVRTouchEvents.AddToTail( code );

	if ( m_vecVRTouchEvents.Count() > 2 )
	{
		const VRTouchEvent_t &touchSampleCurrent = m_vecVRTouchEvents.RemoveFromHead();
		const VRTouchEvent_t &touchSamplePrevious = m_vecVRTouchEvents[ m_vecVRTouchEvents.Head() ];

		float flDeltaY = touchSampleCurrent.m_fValueYRaw - touchSamplePrevious.m_fValueYRaw;
		float flDeltaX = touchSampleCurrent.m_fValueXRaw - touchSamplePrevious.m_fValueXRaw;
		if ( fabs( flDeltaY ) > 0.005f )
		{
			//Msg( "Scroll: %f\n", (-1.0f*flDeltaY* m_nWindowTall) / 2 );
			ScrollPageUp( (-1.0f*flDeltaY* m_nWindowTall) / 2 );
		}
		if ( fabs( flDeltaX ) > 0.005f )
		{
			ScrollPageLeft( ( flDeltaX* m_nWindowWide) / 2 );
		}

		const float k_flLinearMoveThreshold = 0.01f; // how far must they move a finger to count towards a haptics move
		const float k_flTickDistance = 0.2f; // how far must you move your finder to feel a tick
		const int k_nHapticsPulseDuration = 250; // the length/strength of the pulse

		float flMoveDist = sqrtf( flDeltaX*flDeltaX + flDeltaY*flDeltaY );
		if ( flMoveDist > k_flLinearMoveThreshold )
		{
			m_flVRTouchLinearMoveDistanceForHaptics += flMoveDist;

			if ( m_flVRTouchLinearMoveDistanceForHaptics > k_flTickDistance )
			{
				m_flVRTouchLinearMoveDistanceForHaptics = fmod( m_flVRTouchLinearMoveDistanceForHaptics, k_flTickDistance );

				vrapi::VRSystem()->TriggerHapticPulse( touchSampleCurrent.m_nDeviceIndex, 0, k_nHapticsPulseDuration );
			}
		}
	}
#endif
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Reset scroll bars and clear any overflow
//-----------------------------------------------------------------------------
void CHTML::ResetScrollbarsAndClearOverflow()
{
	SetupScrollBar( true, GetActualLayoutWidth() );
	SetupScrollBar( false, GetActualLayoutHeight() );
}


//-----------------------------------------------------------------------------
// Purpose: the mouse moved, tell cef about it 
//-----------------------------------------------------------------------------
void CHTML::OnMouseMove( float flMouseX, float flMouseY )
{
	if ( !( GetParentWindow()->UIWindowInput()->GetInputFocusContext() == UIPanel() || IsDescendantOf( ToPanel2D(GetParentWindow()->UIWindowInput()->GetInputFocusContext()) ) ) )
		return; // ourselves or none of our parents were part of focus, just bail

	if ( BAcceptMouseInput() && flMouseX > 0 && flMouseY > 0 ) // ignore the mouse if the gamepad is in use and make sure it is in the window
	{
		if ( m_bMousePanningActive )
		{
			float flScrollAmountX = ( flMouseX - m_vecMousePanningPos.x );
			float flScrollAmountY = ( flMouseY - m_vecMousePanningPos.y );

			// apply a bezier to the amount if you are in the center 1/4 of the area around the pan cursor, so 1/8th of the screen size
			if ( flScrollAmountX < ( m_nWindowTall/8 ) )
			{
				Vector2D vecReturn;
				m_MousePanBezier.Evaluate( flScrollAmountX/(m_nWindowTall/8), vecReturn );
				flScrollAmountX *= clamp( vecReturn.y, 0.0f, 1.0f);
			}

			if ( flScrollAmountY < ( m_nWindowWide/8 ) )
			{
				Vector2D vecReturn;
				m_MousePanBezier.Evaluate( flScrollAmountY/(m_nWindowWide/8), vecReturn );
				flScrollAmountY *= clamp( vecReturn.y, 0.0f, 1.0f);
			}

			if ( fabs(flScrollAmountX) > k_flMousePanningDeadZone)
			{
				m_bDidMousePanWhileMouseDown = true;
				ScrollPageLeft( clamp( flScrollAmountX * -k_flMousePanningSpeedModifier, -k_flMousePanningMaxSpeed, k_flMousePanningMaxSpeed ) );
			}
			if ( fabs(flScrollAmountY) > k_flMousePanningDeadZone)
			{
				m_bDidMousePanWhileMouseDown = true;
				ScrollPageUp( clamp( flScrollAmountY * -k_flMousePanningSpeedModifier, -k_flMousePanningMaxSpeed, k_flMousePanningMaxSpeed ) );
			}
		}
		else if( m_flCursorX != flMouseX || m_flCursorY != flMouseY )
		{
			OnHTMLCursorMove( flMouseX, flMouseY );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CHTML::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	panorama::RegisterJSMethod( "SetURL", PANORAMA_DELEGATE( &CHTML::OpenURL ) );
	panorama::RegisterJSMethod( "RunJavascript", PANORAMA_DELEGATE( &CHTML::RunJavascript ) );
	panorama::RegisterJSMethod( "SetIgnoreCursor", PANORAMA_DELEGATE( &CHTML::SetIgnoreCursor ) );
}


//-----------------------------------------------------------------------------
// Purpose: the mouse moved, tell cef about it 
//-----------------------------------------------------------------------------
void CHTML::OnHTMLCursorMove( float flCursorX, float flCursorY )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->MouseMove( m_HTMLBrowser, flCursorX - GetHScrollOffset(), flCursorY - GetVScrollOffset() );
	}
#else
	CHTMLProtoBufMsg<CMsgMouseMove> cmd( eHTMLCommands_MouseMove );
	cmd.Body().set_x( flCursorX - GetHScrollOffset() ); // take into account any texture scrolling
	cmd.Body().set_y( flCursorY - GetVScrollOffset() );

	DISPATCH_MESSAGE( eHTMLCommands_MouseMove );
#endif

	m_flCursorX = flCursorX;
	m_flCursorY = flCursorY;

	RequestLinkUnderCursor();
}


//-----------------------------------------------------------------------------
// Purpose: copy the selected text on a html page to the clipboard
//-----------------------------------------------------------------------------
void CHTML::Copy()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->CopyToClipboard( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgCopy> cmd( eHTMLCommands_Copy );
	DISPATCH_MESSAGE( eHTMLCommands_Copy );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: paste from the clipboard into the html page
//-----------------------------------------------------------------------------
void CHTML::Paste()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->PasteFromClipboard( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgPaste> cmd( eHTMLCommands_Paste );
	DISPATCH_MESSAGE( eHTMLCommands_Paste );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: a composed character was entered
//-----------------------------------------------------------------------------
bool CHTML::OnKeyTyped( const KeyData_t &keyData )
{
	if( m_bControlPageScrolling && !m_evtFocus.m_bInput && keyData.m_UniChar == L' ' )
		return true;
	
	// check if we didn't get the key down but did get the char. this happens when you hit enter on
	// the URL bar, keydown activates it and then key typed comes in later.
	if ( !m_bGotKeyDown )
	{
		return false;
	}

	m_bGotKeyDown = false;
	m_evtFocus.m_bUserInputThisPage = true;

	if( keyData.m_UniChar == panorama::SENDTEXT_SPECIALKEY_ENTER )
	{
		PressButton( KEY_ENTER, L'\r' );
	}
	else if( keyData.m_UniChar == panorama::SENDTEXT_SPECIALKEY_BACKSPACE )
	{
		PressButton( KEY_BACKSPACE, L'\b' );
	}
	else
	{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->KeyChar( m_HTMLBrowser, keyData.m_UniChar, ISteamHTMLSurface::k_eHTMLKeyModifier_None );
		}
#else
		CHTMLProtoBufMsg<CMsgKeyChar> cmd( eHTMLCommands_KeyChar );
		cmd.Body().set_unichar( keyData.m_UniChar );
		DISPATCH_MESSAGE( eHTMLCommands_KeyChar );
#endif
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: should we manually scroll?
//-----------------------------------------------------------------------------
bool CHTML::BHandleKeyPressPageScroll() const
{
	return m_bControlPageScrolling && (!m_bPopupVisible && !m_evtFocus.m_bInput );
}


//-----------------------------------------------------------------------------
// Purpose: helper to simulate a button presses
//-----------------------------------------------------------------------------
void CHTML::PressButton( panorama::KeyCode key, uchar32 unichar )
{
	panorama::KeyData_t code;
	code.m_bFirstDown = true;
	code.m_Modifiers = 0;
	code.m_RepeatCount = 0;

	code.m_KeyCode = key;
	code.m_UniChar = 0;
	OnKeyDown( code );

	code.m_KeyCode = KEY_NONE;
	code.m_UniChar = unichar;
	OnKeyTyped( code );

	code.m_KeyCode = key;
	code.m_UniChar = 0;
	OnKeyUp( code );
}


//-----------------------------------------------------------------------------
// Purpose: tell cef a cef was pushed down
//-----------------------------------------------------------------------------
bool CHTML::OnKeyDown( const KeyData_t &code )
{
	bool bForwardKeyToCef = true;
	bool bEatKeyPress = true;

	m_bGotKeyDown = true;

	switch ( code.m_KeyCode )
	{
	case KEY_ENTER:
		if ( m_bPopupVisible )
			break;
		// otherwise fall through to below
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
		// also feed these special keys to the rest of the panel system
		// so UI behaviors still work like tab
		BaseClass::OnKeyDown( code );
		bEatKeyPress = false;
		break;
	case KEY_F5:
		Refresh();
		bEatKeyPress = true;
		bForwardKeyToCef = false;
		break;
	case KEY_TAB:
		// If embedded, let panorama process the tab. Otherwise, let CEF handle tab to navigate pages,
		// but also let it bubble out to the carousel if ctrl or alt are also down
		if ( m_bEmbedded || IsControlPressed( code.m_Modifiers) || IsAltPressed( code.m_Modifiers) )
			bEatKeyPress = false; 
		break;
	case KEY_ESCAPE:
		bForwardKeyToCef = false; // let us handle escape for fullscreen
		bEatKeyPress = false;
		break;
	case KEY_UP:
		bForwardKeyToCef = m_bControlPageScrolling ? m_evtFocus.m_bInput : true; // we scroll the texture, don't let cef try to scroll also
		if ( BHandleKeyPressPageScroll() )
			ScrollPageUp( k_flKeyScrollSpeedMultiplier * MIN( code.m_RepeatCount + 2, 6 ) );
		break;
	case KEY_LEFT:
		// Don't handle if alt or control pressed
		if ( IsControlPressed( code.m_Modifiers) || IsAltPressed( code.m_Modifiers) )
			return false;

		bForwardKeyToCef = m_bControlPageScrolling ? m_evtFocus.m_bInput : true; // we scroll the texture, don't let cef try to scroll also
		if ( BHandleKeyPressPageScroll() )
			ScrollPageLeft( k_flKeyScrollSpeedMultiplier * MIN( code.m_RepeatCount + 2, 6 ) );
		break;
	case KEY_DOWN:
		bForwardKeyToCef = m_bControlPageScrolling ? m_evtFocus.m_bInput : true; // we scroll the texture, don't let cef try to scroll also
		if ( BHandleKeyPressPageScroll() )
			ScrollPageDown( k_flKeyScrollSpeedMultiplier * MIN( code.m_RepeatCount + 2, 6 ) );
		break;
	case KEY_RIGHT:
		// Don't handle if alt or control pressed
		if ( IsControlPressed( code.m_Modifiers) || IsAltPressed( code.m_Modifiers) )
			return false;

		bForwardKeyToCef = m_bControlPageScrolling ? m_evtFocus.m_bInput : true; // we scroll the texture, don't let cef try to scroll also
		if ( BHandleKeyPressPageScroll() )
			ScrollPageRight( k_flKeyScrollSpeedMultiplier * MIN( code.m_RepeatCount + 2, 6 ) );
		break;
	case KEY_PAGEDOWN:
		if ( IsControlPressed( code.m_Modifiers) || IsAltPressed( code.m_Modifiers) )
		{
			bForwardKeyToCef = m_bControlPageScrolling ? false : true; // we scroll the texture, don't let cef try to scroll also
			bEatKeyPress = false; // let CEF handle tab, but also let it bubble out to the carousel if ctrl or alt are also down
			break;
		}
		else if ( BHandleKeyPressPageScroll() )
			ScrollPageUp( -1.0f*m_nWindowTall );
		break;
	case KEY_SPACE:
		bForwardKeyToCef = m_bControlPageScrolling ? m_evtFocus.m_bInput : true; // we scroll the texture, don't let cef try to scroll also
		if ( BHandleKeyPressPageScroll() )
			ScrollPageUp( -1.0f*m_nWindowTall );
		break;
	case KEY_PAGEUP:
		bForwardKeyToCef = m_bControlPageScrolling ? false : true; // we scroll the texture, don't let cef try to scroll also
		if ( IsControlPressed( code.m_Modifiers) || IsAltPressed( code.m_Modifiers) )
		{
			bEatKeyPress = false; // let CEF handle tab, but also let it bubble out to the carousel if ctrl or alt are also down
		}
		else if ( BHandleKeyPressPageScroll() )
			ScrollPageUp( m_nWindowTall );
		break;
	case KEY_HOME:
		bForwardKeyToCef = m_bControlPageScrolling ? m_evtFocus.m_bInput : true; // we scroll the texture, don't let cef try to scroll also
		if ( BHandleKeyPressPageScroll() )
			ScrollPageUp( m_scrollVertical.m_nPageSize + m_ScrollUp.m_flOffsetTextureScroll );
		break;
	case KEY_END:
		bForwardKeyToCef = m_bControlPageScrolling ? m_evtFocus.m_bInput : true; // we scroll the texture, don't let cef try to scroll also
		if ( BHandleKeyPressPageScroll() )
			ScrollPageUp( -1.0f*m_scrollVertical.m_nPageSize + m_nWindowTall );
	case KEY_PAD_PLUS:
	case KEY_EQUAL:
		if ( IsControlPressed( code.m_Modifiers) )
			IncrementPageScale( k_flZoomIncrementKeyBoard );
		break;
	case KEY_PAD_MINUS:
	case KEY_MINUS:
		if ( IsControlPressed( code.m_Modifiers) )
			IncrementPageScale( -1.0*k_flZoomIncrementKeyBoard );
		break;
	default:
		break;
	}
	
	if ( bForwardKeyToCef )
	{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->KeyDown( m_HTMLBrowser, UIEngine()->UIInputEngine()->KeyCodeToWindowsVKey( code.m_KeyCode ), (ISteamHTMLSurface::EHTMLKeyModifiers)GetKeyModifiers( code.m_Modifiers ) );
		}
#else
		CHTMLProtoBufMsg<CMsgKeyDown> cmd( eHTMLCommands_KeyDown );
		cmd.Body().set_keycode( UIEngine()->UIInputEngine()->KeyCodeToWindowsVKey( code.m_KeyCode ) );
		cmd.Body().set_modifiers( GetKeyModifiers( code.m_Modifiers ) );
		DISPATCH_MESSAGE( eHTMLCommands_KeyDown );
#endif
	}

	return bEatKeyPress;
}


//-----------------------------------------------------------------------------
// Purpose: tell cef a key was released
//-----------------------------------------------------------------------------
bool CHTML::OnKeyUp( const KeyData_t & code )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->KeyUp( m_HTMLBrowser, UIEngine()->UIInputEngine()->KeyCodeToWindowsVKey( code.m_KeyCode ), (ISteamHTMLSurface::EHTMLKeyModifiers)GetKeyModifiers( code.m_Modifiers ) );
	}
#else
	CHTMLProtoBufMsg<CMsgKeyUp> cmd( eHTMLCommands_KeyUp );
	cmd.Body().set_keycode( UIEngine()->UIInputEngine()->KeyCodeToWindowsVKey( code.m_KeyCode ) );
	cmd.Body().set_modifiers( GetKeyModifiers( code.m_Modifiers ) );
	DISPATCH_MESSAGE( eHTMLCommands_KeyUp );
#endif
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: pass thru for daisy wheel, tell cef a key was typed
//-----------------------------------------------------------------------------
void CHTML::InsertCharacterAtCursor( const uchar32 &unichar ) 
{  
	m_bGotKeyDown = true;
	KeyData_t data = {};
	data.m_eSource = k_ePanelEventSourceProgram;
	data.m_UniChar = unichar;
	data.m_bFirstDown = true;
	OnKeyTyped( data );
}


//-----------------------------------------------------------------------------
// Purpose: scroll the web page by this many pixels
//-----------------------------------------------------------------------------
bool CHTML::ScrollHelper( bool bHorizontal, float flScrollDelta, float flLeadInTime, bool bAllowOverScroll, bool * bHitEdge )
{
	if ( flScrollDelta == 0.0f )
		return false;
		
	if( m_bFullScreen || m_bIgnoreCursor )
		bAllowOverScroll = false; // no need to overscroll

	ScrollControl_t &scrollControl = bHorizontal ? m_ScrollLeft : m_ScrollUp;
	const ScrollData_t &scrollBar = bHorizontal ? m_scrollHorizontal : m_scrollVertical;
	const float flMaxTextureScroll = (bHorizontal ? m_nWindowWide : m_nWindowTall) / 2;
	const float flScrollTextureCurrent = scrollControl.m_flOffsetTextureScroll;
	const int nScrollHTMLCurrent = scrollBar.m_nScroll;
	const int nScrollHTMLMax = scrollBar.m_nMaxScroll;
			
	bool bScrollBefore = scrollControl.m_bScrollingUp;
	scrollControl.m_bScrollingUp = false;
	if ( flScrollDelta < 0.0f )
	{
		scrollControl.m_bScrollingUp = true;
	}

	// 
	// Step 1. Scale the actual scroll to a bezier during a startup phase
	// 
	double flCurTime = Plat_FloatTime();
	if ( scrollControl.m_flLastScrollTime == 0.0f || scrollControl.m_bScrollingUp != bScrollBefore )
		scrollControl.m_flLastScrollTime = flCurTime;

	// if we are in the startup of a scroll then scale movement
	if( flLeadInTime > 0.0f && flCurTime >= scrollControl.m_flLastScrollTime && (flCurTime - scrollControl.m_flLastScrollTime) < flLeadInTime )
	{
		// this used to be a Bezier, now just linear
		flScrollDelta *= (flCurTime - scrollControl.m_flLastScrollTime) / flLeadInTime;
	}
	else
	{
		scrollControl.m_flLastScrollTime = flCurTime + flLeadInTime; // otherwise we are in the middle of a scroll, keep moving
	}

	if ( flScrollDelta == 0.0f )
		return false;

	//	Plat_OutputDebugString( "ScrollHelper %s: flScrollDelta:%4.1f, m_flOffsetTextureScroll:%4.1f, nHTMLScrollCurrent:%4d, nHTMLScrollMax:%4d\n",
	//		bHorizontal ? "Horizontal" : "Vertical  ", flScrollDelta, scrollControl.m_flOffsetTextureScroll, nScrollHTMLCurrent, nScrollHTMLMax );

	// 
	// Step 2. If we have moved the texture around at all see if we should undo some of that
	// 
	if ( scrollControl.m_flOffsetTextureScroll != 0.0f  )
	{
		scrollControl.m_flOffsetTextureScroll -= flScrollDelta;
		scrollControl.m_flOffsetTextureScroll = clamp( scrollControl.m_flOffsetTextureScroll, -1.0f * flMaxTextureScroll, flMaxTextureScroll );

		// if we just used up all the extra overscroll then push it back into the scroll delta
		if ( ( scrollControl.m_flOffsetTextureScroll > 0.0f && flScrollDelta < 0.0f ) ||
			 ( scrollControl.m_flOffsetTextureScroll < 0.0f && flScrollDelta > 0.0f ) )
		{
			flScrollDelta -= scrollControl.m_flOffsetTextureScroll;
			scrollControl.m_flOffsetTextureScroll = 0.0f;
		}
		else
		{
			// all scrolling happened on the texture, we are done
						
			if( bAllowOverScroll )
			{
				// tell cef the mouse is now over the middle of the screen
				OnHTMLCursorMove( GetActualLayoutWidth() / 2.0f, GetActualLayoutHeight() / 2.0f );
			}

			return true;
		}
	}

	// 
	// Step 3. Now scroll the html page itself
	// 
	flScrollDelta /= m_flZoom; // CEF scroll amounts are unzoomed space

	scrollControl.m_flScrollRemainder += fmod( flScrollDelta, 1.0f );

	int nScrollHTMLPos = (int) flScrollDelta + nScrollHTMLCurrent;

	if( fabs( scrollControl.m_flScrollRemainder ) > 1.0f )
	{
		nScrollHTMLPos += (int) scrollControl.m_flScrollRemainder;
		scrollControl.m_flScrollRemainder = fmod( scrollControl.m_flScrollRemainder, 1.0f );
	}
	
	bool bWouldHitEdge = false;

	// 
	// Step 4. Clamp scrolling the html page to its scroll bounds
	// 
	if( nScrollHTMLPos >= nScrollHTMLMax )
	{
		if ( bAllowOverScroll )
		{
			scrollControl.m_flOffsetTextureScroll += (nScrollHTMLMax - nScrollHTMLPos ) * m_flZoom;
			scrollControl.m_flOffsetTextureScroll = clamp( scrollControl.m_flOffsetTextureScroll, -1.0f * flMaxTextureScroll, flMaxTextureScroll );
		}

		nScrollHTMLPos = nScrollHTMLMax;
		scrollControl.m_flScrollRemainder = 0.0f;
		
		bWouldHitEdge = true;
	}
	else if( nScrollHTMLPos < 0 )
	{
		if ( bAllowOverScroll )
		{
			scrollControl.m_flOffsetTextureScroll -= ( nScrollHTMLPos*m_flZoom);
			scrollControl.m_flOffsetTextureScroll = clamp( scrollControl.m_flOffsetTextureScroll, -1.0f * flMaxTextureScroll, flMaxTextureScroll );
		}

		nScrollHTMLPos = 0;
		scrollControl.m_flScrollRemainder = 0.0f;
		
		bWouldHitEdge = true;
	}

	if( bAllowOverScroll && flScrollTextureCurrent != scrollControl.m_flOffsetTextureScroll )
	{
		// we did a texture scroll, update cursor position 
		OnHTMLCursorMove( GetActualLayoutWidth() / 2.0f, GetActualLayoutHeight() / 2.0f );
	}

	if( nScrollHTMLPos != nScrollHTMLCurrent )
	{
		// HTML scroll needed
		if( bHorizontal )
		{
			SetHorizontalScroll( nScrollHTMLPos );
		}
		else
		{
			SetVerticalScroll( nScrollHTMLPos );
		}

		RequestLinkUnderCursor();

		UpdatePanoramaScrollBars();
		
		
		if ( bWouldHitEdge && bHitEdge )
			*bHitEdge = true;
	}
	
	return true;
	
}


//-----------------------------------------------------------------------------
// Purpose: scroll the web page left by this many pixels
//-----------------------------------------------------------------------------
void CHTML::UpdatePanoramaScrollBars()
{
	if ( m_pHorizontalScrollBar )
	{
		m_pHorizontalScrollBar->SetScrollWindowPosition( m_scrollHorizontal.m_nScroll );
		m_pHorizontalScrollBar->Normalize();
	}

	if ( m_pVerticalScrollBar )
	{
		m_pVerticalScrollBar->SetScrollWindowPosition( m_scrollVertical.m_nScroll );
		m_pVerticalScrollBar->Normalize();
	}
}


//-----------------------------------------------------------------------------
// Purpose: scroll the web page left by this many pixels
//-----------------------------------------------------------------------------
void CHTML::ScrollPageLeft( float flScrollValue )
{
	ScrollHelper( true, -1.0*flScrollValue );
}


//-----------------------------------------------------------------------------
// Purpose: scroll the web page right by this many pixels
//-----------------------------------------------------------------------------
void CHTML::ScrollPageRight( float flScrollValue )
{
	ScrollHelper( true, flScrollValue );
}

//-----------------------------------------------------------------------------
// Purpose: scroll the web page down by this many pixels
//-----------------------------------------------------------------------------
void CHTML::ScrollPageDown( float flScrollValue )
{
	ScrollHelper( false, flScrollValue );
}


//-----------------------------------------------------------------------------
// Purpose: scroll the web page up by this many pixels
//-----------------------------------------------------------------------------
void CHTML::ScrollPageUp( float flScrollValue )
{
	ScrollHelper( false, -1.0*flScrollValue );
}


//-----------------------------------------------------------------------------
// Purpose: scroll the web page up to a certain percentage offset
//-----------------------------------------------------------------------------
void CHTML::ScrollToXPercent( float flXPercent )
{
	if ( flXPercent > 1.0f  || flXPercent < 0.0f )
		return;

	// work out the delta change from the current scroll pos (m_flOffsetTextureScroll) and the final value they want to get to in the page given percent offset
	float flScrollValue = (m_scrollHorizontal.m_nPageSize+m_nHTMLPageWide/2)*flXPercent - m_nHTMLPageWide/2 - m_ScrollLeft.m_flOffsetTextureScroll;

	ScrollHelper( true, flScrollValue  );
}


//-----------------------------------------------------------------------------
// Purpose: scroll the web page down to a certain percentage offset
//-----------------------------------------------------------------------------
void CHTML::ScrollToYPercent( float flYPercent )
{
	if ( flYPercent > 1.0f  || flYPercent < 0.0f )
		return;

	// work out the delta change from the current scroll pos (m_flOffsetTextureScroll) and the final value they want to get to in the page given percent offset
	float flScrollValue = (m_scrollVertical.m_nPageSize+m_nHTMLPageTall/2)*flYPercent - m_nHTMLPageTall/2 - m_ScrollUp.m_flOffsetTextureScroll;

	ScrollHelper( false, flScrollValue, false, false );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTML::OnGamePadDownImpl( const GamePadData_t &code, bool *out_pbOptionalResult /* = nullptr */ )
{
	bool bDummyUnusedResult;
	if ( !out_pbOptionalResult )
	{
		out_pbOptionalResult = &bDummyUnusedResult;
	}

	switch ( code.m_GamePadCode )
	{
    case XK_STICK1_DOWN:
	case XK_STICK1_UP:
	case XK_STICK1_LEFT:
	case XK_STICK1_RIGHT:
		// Let panorama handle XK_STICK1 if embedded, otherwise we consume the analog stick1 input so lets ignore the key presses coming from it
		*out_pbOptionalResult = !m_bEmbedded;
		return true;
	case STEAM_LEFTSTICK_UP:
	case STEAM_LEFTSTICK_DOWN:
	case STEAM_LEFTSTICK_LEFT:
	case STEAM_LEFTSTICK_RIGHT:
		// Let panorama handle STEAM_LEFTSTICK if embedded, otherwise we consume the analog stick1 input so lets ignore the key presses coming from it
		*out_pbOptionalResult = !m_bEmbedded;
		return true;
	case STEAM_LEFTPAD_UP:
	case STEAM_LEFTPAD_DOWN:
	case STEAM_LEFTPAD_RIGHT:
	case STEAM_LEFTPAD_LEFT:
		// Left pad is always used for scrolling
		*out_pbOptionalResult = true;
		return true;
	case STEAM_BUTTON_DPAD_UP:
	case STEAM_BUTTON_DPAD_DOWN:
	case STEAM_BUTTON_DPAD_LEFT:
	case STEAM_BUTTON_DPAD_RIGHT:
		// Same as above, since this means clicking the left pad
		*out_pbOptionalResult = true;
		return true;
	case STEAM_RIGHTPAD_UP:
	case STEAM_RIGHTPAD_DOWN:
	case STEAM_RIGHTPAD_RIGHT:
	case STEAM_RIGHTPAD_LEFT:
		// Right pad is only used for zooming, which is disabled in embedded views
		*out_pbOptionalResult = !m_bEmbedded;
		return true;
	case XK_STICK2_DOWN:
	case XK_STICK2_UP:
	case XK_STICK2_LEFT:
	case XK_STICK2_RIGHT:
		// we consume the analog stick2 input so lets ignore the key presses coming from it
		*out_pbOptionalResult = true;
		return true;

	case STEAM_RTRIGGER_ANALOG:
		if ( code.m_RepeatCount )
		{
			*out_pbOptionalResult = true;
			return true;
		}
		UIEngine()->PulseActiveControllerHaptic(
			IUIEngine::k_EHapticFeedbackPosition_Right,
			IUIEngine::k_EHapticFeedbackStrength_High );
	case XK_BUTTON_A:
	case STEAM_BUTTON_A:
	{
		m_evtFocus.m_bUserInputThisPage = true;
		m_bFocusEventSentForClick = false;
		if ( m_LinkAtPos.m_bInput && m_evtFocus.m_bInput )
		{
			DispatchEventAsync( k_flFormFocusDelayCheck, HTMLFormFocusPending(), this ); // queue a call to check in 1 sec that we sent a form focus event
		}

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->MouseDown( m_HTMLBrowser, ISteamHTMLSurface::eHTMLMouseButton_Left );
			SteamHTMLSurface()->MouseUp( m_HTMLBrowser, ISteamHTMLSurface::eHTMLMouseButton_Left );
		}
#else
		{
			CHTMLProtoBufMsg<CMsgMouseDown> cmd( eHTMLCommands_MouseDown );
			cmd.Body().set_mouse_button( MOUSE_LEFT );
			DISPATCH_MESSAGE( eHTMLCommands_MouseDown );
		}
		{
			CHTMLProtoBufMsg<CMsgMouseUp> cmd( eHTMLCommands_MouseUp );
			cmd.Body().set_mouse_button( MOUSE_LEFT );
			DISPATCH_MESSAGE( eHTMLCommands_MouseUp );
		}
#endif
		*out_pbOptionalResult = true;
		
		if ( code.m_GamePadCode == STEAM_BUTTON_A )
		{
			if ( m_bEmbedded )
			{
				OnTabForward( 0 );
				*out_pbOptionalResult = true;
				return true;
			}
		}
		return true;
	}
	// intentional fall-through
	case XK_BUTTON_STICK1:
		if ( m_LinkAtPos.m_bLiveLink )
		{
			DispatchEventAsync( 0.0f, HTMLOpenLinkInNewTab(), this, m_LinkAtPos.m_sURL );
			*out_pbOptionalResult = true;
			return true;
		}
		break;
	case STEAM_BUTTON_LPAD_TOUCH:
		{
			m_bVerticalAxisSnap = true;

			*out_pbOptionalResult = true;
			return true;
		}
	case STEAM_BUTTON_RPAD_TOUCH:
		{
			m_flInitialZoomLevel = m_flZoom;
			m_bMarkZoomStart = true;
			
			*out_pbOptionalResult = true;
			return true;
		}
	case STEAM_BUTTON_LPAD_CLICKED:
		{
			m_bClickingLeftPad = true;
			m_bMarkZoomStart = true;
			m_flInitialZoomLevel = m_flZoom;
			
			*out_pbOptionalResult = true;
			return true;
		}

	case VR_BUTTON_PRIMARY_UP:
		{
			if ( BHandleKeyPressPageScroll() )
				ScrollPageUp( k_flKeyScrollSpeedMultiplier * MIN( code.m_RepeatCount + 2, 6 ) );

			*out_pbOptionalResult = true;
			return true;
		}
	case VR_BUTTON_PRIMARY_DOWN:
	{
		if ( BHandleKeyPressPageScroll() )
			ScrollPageDown( k_flKeyScrollSpeedMultiplier * MIN( code.m_RepeatCount + 2, 6 ) );

		*out_pbOptionalResult = true;
		return true;
	}

	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: let button a feed through as a mouse click
//-----------------------------------------------------------------------------
bool CHTML::OnGamePadDown( const GamePadData_t &code )
{
	bool bReturnResult;
	if ( OnGamePadDownImpl( code, &bReturnResult ) )
		return bReturnResult;

	return BaseClass::OnGamePadDown( code );
}


//-----------------------------------------------------------------------------
// Purpose: gamepad digital up handler
//-----------------------------------------------------------------------------
bool CHTML::OnGamePadUp( const GamePadData_t &code )
{
	return BaseClass::OnGamePadUp( code );
}




//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHTML::ProcessAnalogScroll( float fValueX, float fValueY, float fDeadzoneValue, bool bAllowOverScroll )
{
	// we assume fValue goes from -1000 to +1000, scroll 15 pixels max
	static const float s_fMaxScrollSpeed = 15.0f;
		
	const float fDeadzoneTestValue = fDeadzoneValue * k_fScrollDeadzoneScale;

	if( fabsf( fValueY ) > fDeadzoneTestValue )
	{
		ScrollHelper( false, (-s_fMaxScrollSpeed * fValueY) / 1000.0f, k_flScrollLeadInTimeAnalog, bAllowOverScroll );
	}

	if( fabsf( fValueX ) > fDeadzoneTestValue )
	{
		ScrollHelper( true, (s_fMaxScrollSpeed * fValueX) / 1000.0f, k_flScrollLeadInTimeAnalog, bAllowOverScroll );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHTML::ProcessAnalogZoom( float fValueX, float fValueY, float fDeadzoneValue )
{
	if( fabsf( fValueX ) <= fDeadzoneValue  && fabsf( fValueY ) <= fDeadzoneValue )
	{
		m_flStartZoomTime = 0.0f;
		return;
	}

	const double flNow = Plat_FloatTime();
	double fTimeDelta = 0.0f;

	if( m_flStartZoomTime <= 0.0f || m_flStartZoomTime > flNow )
	{
		m_flStartZoomTime = flNow; // starting now
	}
	else
	{
		fTimeDelta = flNow - m_flStartZoomTime;
	}

	float fZoomDelta = fabsf( fValueX ) > fabsf( fValueY )
		? copysign( k_flZoomIncrement, fValueX )
		: copysign( k_flZoomIncrement, fValueY );

	Vector2D vecReturn;
	if( fTimeDelta > 0.0 && fTimeDelta < k_flZoomlLeadInTime )
	{
		m_ScrollBezier.Evaluate( fTimeDelta / k_flZoomlLeadInTime, vecReturn );
		fZoomDelta *= vecReturn.y;
	}

	if( fZoomDelta != 0.0f )
	{
		IncrementPageScale( fZoomDelta );
	}
}


//-----------------------------------------------------------------------------
// Purpose: XInput analog processing
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTML::OnGamePadAnalogImpl( const GamePadData_t &code, bool *out_pbOptionalResult /* = nullptr */ )
{
	enum ECommand
	{
		kCommand_Nothing,						// we recognize this input but don't want to do do anything with it
		kCommand_AnalogScroll,					// input coming from analog sticks (ie., 360)
		kCommand_AnalogScrollWithoutOverscroll,	// input coming from Steam analog stick
		kCommand_AnalogZoom,
	};

	ECommand eCommand = kCommand_Nothing;

	if ( m_bEmbedded )
	{
		switch ( code.m_GamePadCode )
		{
		// For embedded panels on a 360 controller, the right analog stick controls movement. The left analog
		// stick is still used for general navigation through the UI so we ignore it here.
		case XK_STICK1_ANALOG:			eCommand = kCommand_Nothing;				break;
		case XK_STICK2_ANALOG:			eCommand = kCommand_AnalogScroll;			break;

		// left stick is always used for scrolling
		case STEAM_LEFTSTICK_ANALOG:	eCommand = kCommand_AnalogScrollWithoutOverscroll;	break;
		}
	}
	else
	{
		switch ( code.m_GamePadCode )
		{
		// For non-embedded panels on a 360 controller, the left analog stick controls movement, as the "UI" is now
		// a fullscreen browser. The right analog stick is now used for zoom.
		case XK_STICK1_ANALOG:			eCommand = kCommand_AnalogScroll;			break;
		case XK_STICK2_ANALOG:			eCommand = kCommand_AnalogZoom;				break;

		// left stick is always used for scrolling
		case STEAM_LEFTSTICK_ANALOG:	eCommand = kCommand_AnalogScrollWithoutOverscroll;	break;
		}
	}

	// If we got any form of analog input that we don't know how to process, let the rest of our control
	// hierarchy deal with it.
	if ( eCommand == kCommand_Nothing )
		return false;

	// Process analog scroll/zoom. These functions will return whether the input actually resulted in an
	// action taking place (considering deadzone, etc.). If they didn't, we reset our timer so that the
	// next actually-counted input is considered fresh.
			
	if ( eCommand == kCommand_AnalogScroll || eCommand == kCommand_AnalogScrollWithoutOverscroll )
	{
		bool bAllowOverScroll = (eCommand != kCommand_AnalogScrollWithoutOverscroll);

		const float fDeadzoneValue = UIInputEngine()->GetDeadZoneValue( code.m_GamePadCode );
		ProcessAnalogScroll( code.m_fValueX, code.m_fValueY, fDeadzoneValue, bAllowOverScroll );
	}
	else if( eCommand == kCommand_AnalogZoom )
	{
		const float fDeadzoneValue = UIInputEngine()->GetDeadZoneValue( code.m_GamePadCode );
		ProcessAnalogZoom( code.m_fValueX, code.m_fValueY, fDeadzoneValue );
	}
	
	if ( out_pbOptionalResult )
	{
		*out_pbOptionalResult = true;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTML::OnGamePadAnalog( const GamePadData_t &code )
{
	bool bReturnResult;
	if ( OnGamePadAnalogImpl( code, &bReturnResult ) )
		return bReturnResult;

	return BaseClass::OnGamePadAnalog( code );
}


//-----------------------------------------------------------------------------
// Purpose: run this javascript block on the currentload loaded html page
//-----------------------------------------------------------------------------
void CHTML::RunJavascript( const char *pchScript )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->ExecuteJavascript( m_HTMLBrowser, pchScript );
	}
#else
	CHTMLProtoBufMsg<CMsgExecuteJavaScript> cmd( eHTMLCommands_ExecuteJavaScript );
	cmd.Body().set_script( pchScript );
	DISPATCH_MESSAGE( eHTMLCommands_ExecuteJavaScript );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: open this url with this explicit post data, used for things like logins
//-----------------------------------------------------------------------------
void CHTML::PostURL( const char *pchURL, const char *pchPostData )
{
	if ( !pchURL )
	{
		pchURL = "about:blank";
	}

	if ( !m_bReady )
	{
		m_sURLToLoad = pchURL;
		m_sURLPostData = pchPostData;
	}
	else
	{
		for ( int i = 0; i < UIEngine()->GetXHeaderCount(); i++ )
		{
			CUtlString strName;
			CUtlString strValue;
			UIEngine()->GetXHeader( i, strName, strValue );
			AddHeader( strName, strValue );
		}

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->LoadURL( m_HTMLBrowser, pchURL, pchPostData );
		}
#else
		CHTMLProtoBufMsg<CMsgPostURL> cmd( eHTMLCommands_PostURL );
		cmd.Body().set_url( pchURL );
		cmd.Body().set_pageserial( ++m_PageLoadCount );
		cmd.Body().set_post( pchPostData );

		DISPATCH_MESSAGE( eHTMLCommands_PostURL );
#endif

		m_sCurrentURL = pchURL;
		ReleaseTextureMemory();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a custom header to all requests
//-----------------------------------------------------------------------------
void CHTML::AddHeader( const char *pchHeader, const char *pchValue )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->AddHeader( m_HTMLBrowser, pchHeader, pchValue );
	}
#else
	CHTMLProtoBufMsg<CMsgAddHeader> cmd( eHTMLCommands_AddHeader );
	cmd.Body().set_key( pchHeader );
	cmd.Body().set_value( pchValue );
	DISPATCH_MESSAGE( eHTMLCommands_AddHeader );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: the user has selected a filename from a file open dialog
//-----------------------------------------------------------------------------
void CHTML::SetFileDialogChoice( const char *pchFileName )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		const char * arrayFileName[2] = { pchFileName, nullptr };
		SteamHTMLSurface()->FileLoadDialogResponse( m_HTMLBrowser, arrayFileName );
	}
#else
	CHTMLProtoBufMsg<CMsgFileLoadDialogResponse> cmd( eHTMLCommands_FileLoadDialogResponse );
	if ( pchFileName )
		cmd.Body().add_files( pchFileName );

	DISPATCH_MESSAGE( eHTMLCommands_FileLoadDialogResponse );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: hide a popup menu
//-----------------------------------------------------------------------------
void CHTML::HidePopup()
{
	if ( m_bPopupVisible )
	{
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
		CHTMLProtoBufMsg<CMsgHidePopup> cmd( eHTMLCommands_HidePopup );
		DISPATCH_MESSAGE( eHTMLCommands_HidePopup );
#endif
		m_bPopupVisible = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: tell the html control it has key focus
//-----------------------------------------------------------------------------
void CHTML::SetHTMLFocus()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetKeyFocus( m_HTMLBrowser, true );
	}
#else
	CHTMLProtoBufMsg<CMsgSetFocus> cmd( eHTMLCommands_SetFocus );
	cmd.Body().set_focus( true );
	DISPATCH_MESSAGE( eHTMLCommands_SetFocus );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: you have lost key focus
//-----------------------------------------------------------------------------
void CHTML::KillHTMLFocus()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetKeyFocus( m_HTMLBrowser, false );
	}
#else
	CHTMLProtoBufMsg<CMsgSetFocus> cmd( eHTMLCommands_SetFocus );
	cmd.Body().set_focus( false );
	DISPATCH_MESSAGE( eHTMLCommands_SetFocus );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: find this utf8 string in the page
//-----------------------------------------------------------------------------
void CHTML::Find( const char *pchSubStr )
{
	if ( m_sLastSearchString != pchSubStr )
		m_bInFind = false;

	m_sLastSearchString = pchSubStr;

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->Find( m_HTMLBrowser, pchSubStr, m_bInFind, false );
	}
#else
	CHTMLProtoBufMsg<CMsgFind> cmd( eHTMLCommands_Find );
	cmd.Body().set_find( pchSubStr );
	cmd.Body().set_infind( m_bInFind );
	DISPATCH_MESSAGE( eHTMLCommands_Find );
#endif
	m_bInFind = true;
}


//-----------------------------------------------------------------------------
// Purpose: given an existing find get the next entry in order
//-----------------------------------------------------------------------------
void CHTML::FindNext()
{
	if ( m_bInFind )
		Find( m_sLastSearchString );
}

//-----------------------------------------------------------------------------
// Purpose: find the previous entry given being in find already
//-----------------------------------------------------------------------------
void CHTML::FindPrevious()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->Find( m_HTMLBrowser, m_sLastSearchString, m_bInFind, true );
	}
#else
	CHTMLProtoBufMsg<CMsgFind> cmd( eHTMLCommands_Find );
	cmd.Body().set_find( m_sLastSearchString );
	cmd.Body().set_infind( m_bInFind );
	DISPATCH_MESSAGE( eHTMLCommands_Find );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: if in find then stop it
//-----------------------------------------------------------------------------
void CHTML::StopFind()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->StopFind( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgStopFind> cmd( eHTMLCommands_StopFind );
	DISPATCH_MESSAGE( eHTMLCommands_StopFind );
#endif
	m_bInFind = false;
}


//-----------------------------------------------------------------------------
// Purpose: request a callback for the url link at cursor position
//-----------------------------------------------------------------------------
void CHTML::RequestLinkUnderCursor()
{
	if( m_bIgnoreCursor )
		return;

	// viewport coords
	const int x = m_flCursorX - GetHScrollOffset();
	const int y = m_flCursorY - GetVScrollOffset();

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->GetLinkAtPosition( m_HTMLBrowser, x, y );
	}
#else
	CHTMLProtoBufMsg<CMsgLinkAtPosition> cmd( eHTMLCommands_LinkAtPosition );
	cmd.Body().set_x( x ); 
	cmd.Body().set_y( y );
	DISPATCH_MESSAGE( eHTMLCommands_LinkAtPosition );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: increment the page scale, negative values zoom out
//-----------------------------------------------------------------------------
void CHTML::IncrementPageScale( float flScaleIncrement, bool bZoomFromOrigin )
{
	if ( m_bWaitingForZoomResponse || fabs(flScaleIncrement) < 0.005f )
		return;

	float flNewZoom = clamp( m_flZoom + flScaleIncrement, k_flZoomMinimum, k_flMaxZoomLevel );

	if( flNewZoom == m_flZoom )
		return;

	m_bWaitingForZoomResponse = true; // only allow 1 outstanding zoom request, so that we are correctly setting the position to zoom around
	
	int x, y; // viewport coords
	if( bZoomFromOrigin )
	{
		x = GetActualLayoutWidth() / 2;
		y = GetActualLayoutHeight() / 2;
	}
	else
	{
		x = m_flCursorX - GetHScrollOffset();
		y = m_flCursorY - GetVScrollOffset();
	}

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetPageScaleFactor( m_HTMLBrowser, flNewZoom, x, y );
	}
#else
	CHTMLProtoBufMsg<CMsgScalePageToValue> cmd( eHTMLCommands_SetPageScale );
	cmd.Body().set_scale( flNewZoom );
	cmd.Body().set_x( x );
	cmd.Body().set_y( y );
	DISPATCH_MESSAGE( eHTMLCommands_SetPageScale );
#endif
}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: scale was incremented, got a new zoom
//-----------------------------------------------------------------------------
void CHTML::BrowserScalePageToValueResponse( const CMsgScalePageToValueResponse *pCmd ) 
{
	m_bWaitingForZoomResponse = false;
	m_flZoom = pCmd->zoom();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: zoom the page to the element under the gamepad in the middle of the scren
//-----------------------------------------------------------------------------
void CHTML::ZoomToElementUnderCursor()
{
	m_bWaitingForZoomResponse = true;

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	Assert( !"Not implemented/supported" );
#else
	CHTMLProtoBufMsg<CMsgZoomToElementAtPosition> cmd( eHTMLCommands_ZoomToElementAtPosition );
	cmd.Body().set_x( m_flCursorX - GetHScrollOffset() ); // viewport coords
	cmd.Body().set_y( m_flCursorY - GetVScrollOffset() );
	DISPATCH_MESSAGE( eHTMLCommands_ZoomToElementAtPosition );
#endif
	RefreshTextureMemory();
}


//-----------------------------------------------------------------------------
// Purpose:DISABLED:  zoom the page to the element under this x,y location, relative to the html page
//-----------------------------------------------------------------------------
bool CHTML::OnHTMLCommitZoom( const panorama::CPanelPtr< IUIPanel > &ptrPanel, float flZoom )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetPageScaleFactor( m_HTMLBrowser, flZoom, -1, -1 );
	}
#else
	CHTMLProtoBufMsg<CMsgScalePageToValue> cmd( eHTMLCommands_SetPageScale );
	cmd.Body().set_scale( flZoom );
	cmd.Body().set_x( -1 );
	cmd.Body().set_y( -1 );
	DISPATCH_MESSAGE( eHTMLCommands_SetPageScale );
#endif
	m_bWaitingForZoomResponse = true;
	m_flZoom = flZoom;
	return true;
}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: zoomed the page to the element under this x,y location, relative to the html page
//-----------------------------------------------------------------------------
void CHTML::BrowserZoomToElementAtPositionResponse( const CMsgZoomToElementAtPositionResponse *pCmd )
{
	// half way through the zoom commit the new page scale
	m_bWaitingForZoomResponse = false;
	m_flZoom = pCmd->scale();
	// DISABLED: UIEngine()->DispatchEventAsync( k_flZoomInTime/2.0f, HTMLCommitZoom::MakeEvent( this, pCmd->scale() ) );
	UISoundSystem()->PlaySound( "zoom_in", k_ESoundType_Effects );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: reply to a previous RequestLinkAtPosition call, cache off the last returned value and fire an event
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
void CHTML::OnLinkAtPositionResponse( HTML_LinkAtPosition_t *pCmd )
{
	if ( pCmd->unBrowserHandle != m_HTMLBrowser ) return;

	m_LinkAtPos.m_sURL = pCmd->pchURL;
	m_LinkAtPos.m_bLiveLink = pCmd->bLiveLink && !m_bIgnoreCursor;
	m_LinkAtPos.m_bInput = pCmd->bInput;
	DispatchEvent( HMTLLinkAtPosition(), this, m_LinkAtPos.m_sURL.String(), m_LinkAtPos.m_bLiveLink );

	GetParentWindow()->SetMouseCursor( BIsCursorOverLink() ? eMouseCursor_Hand : eMouseCursor_Arrow );
}
#else
void CHTML::BrowserLinkAtPositionResponse( const CMsgLinkAtPositionResponse *pCmd )
{
	m_LinkAtPos.m_sURL = pCmd->url().c_str();
	m_LinkAtPos.m_bLiveLink = pCmd->blivelink() && !m_bIgnoreCursor;
	m_LinkAtPos.m_bInput = pCmd->binput();
	DispatchEvent( HMTLLinkAtPosition(), this, m_LinkAtPos.m_sURL.String(), m_LinkAtPos.m_bLiveLink );

	GetParentWindow()->SetMouseCursor( BIsCursorOverLink() ? eMouseCursor_Hand : eMouseCursor_Arrow );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: how much scroll do we have
//-----------------------------------------------------------------------------
int CHTML::HorizontalScroll()
{
	return m_scrollHorizontal.m_nScroll;
}


//-----------------------------------------------------------------------------
// Purpose: how much scroll do we have
//-----------------------------------------------------------------------------
int CHTML::VerticalScroll()
{
	return m_scrollVertical.m_nScroll;
}


//-----------------------------------------------------------------------------
// Page size
//-----------------------------------------------------------------------------
int CHTML::HorizontalPageSize()
{
	return m_scrollHorizontal.m_nPageSize;
}

int CHTML::VerticalPageSize()
{
	return m_scrollVertical.m_nPageSize;
}


//-----------------------------------------------------------------------------
// Purpose: should we show this scrollbar
//-----------------------------------------------------------------------------
bool CHTML::IsHorizontalScrollBarVisible()
{
	return m_scrollHorizontal.m_bVisible;
}


//-----------------------------------------------------------------------------
// Purpose: should we show this scrollbar
//-----------------------------------------------------------------------------
bool CHTML::IsVeritcalScrollBarVisible()
{
	return m_scrollVertical.m_bVisible;
}


//-----------------------------------------------------------------------------
// Purpose: scroll to this pixel position in the page
//-----------------------------------------------------------------------------
void CHTML::SetHorizontalScroll( int scroll )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetHorizontalScroll( m_HTMLBrowser, scroll );
	}
#else
	CHTMLProtoBufMsg<CMsgSetHorizontalScroll> cmd( eHTMLCommands_SetHorizontalScroll );
	cmd.Body().set_scroll( scroll );
	DISPATCH_MESSAGE( eHTMLCommands_SetHorizontalScroll );
#endif
	m_scrollHorizontal.m_nScroll = scroll;
}


//-----------------------------------------------------------------------------
// Purpose: scroll to this pixel position in the page
//-----------------------------------------------------------------------------
void CHTML::SetVerticalScroll( int scroll )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetVerticalScroll( m_HTMLBrowser, scroll );
	}
#else
	CHTMLProtoBufMsg<CMsgSetVerticalScroll> cmd( eHTMLCommands_SetVerticalScroll );
	cmd.Body().set_scroll( scroll );
	DISPATCH_MESSAGE( eHTMLCommands_SetVerticalScroll );
#endif
	m_scrollVertical.m_nScroll = scroll;
}


//-----------------------------------------------------------------------------
// Purpose: pop this page's source in notepad
//-----------------------------------------------------------------------------
void CHTML::ViewSource()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->ViewSource( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgViewSource> cmd( eHTMLCommands_ViewSource );
	DISPATCH_MESSAGE( eHTMLCommands_ViewSource );
#endif
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CHTML::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	BaseClass::ValidateClientPanel( validator, pchName );
	ValidateObj( m_sHTMLTitle );
	ValidateObj( m_sLastSearchString );
	ValidateObj( m_sURLToLoad );
	ValidateObj( m_sCurrentURL );
	ValidateObj( m_LinkAtPos.m_sURL );

	ValidateObj( m_evtFocus.m_sInputType );
	ValidateObj( m_evtFocus.m_sSearchLabel );
	ValidateObj( m_evtFocus.m_sName );

	ValidatePtr( m_pHorizontalScrollBar );
	ValidatePtr( m_pVerticalScrollBar );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: we are navigating to a new url
//-----------------------------------------------------------------------------
void CHTML::OnURLChanged( const char *url, const char *pchPostData, bool bIsRedirect )
{
	m_sCurrentURL= url;
}


//-----------------------------------------------------------------------------
// Purpose: the page with this url is finished loading
//-----------------------------------------------------------------------------
void CHTML::OnFinishRequest( const char *url, const char *pageTitle )
{
}


//-----------------------------------------------------------------------------
// Purpose: the page with this url is finished loading
//-----------------------------------------------------------------------------
void CHTML::OnPageLoaded(const char *url, const char *pageTitle, const CUtlMap < CUtlString, CUtlString > &headers )
{
	BaseClass::FirePanelLoadedEvent();
}


//-----------------------------------------------------------------------------
// Purpose: cef wants to start loading this url, do we let it?
//-----------------------------------------------------------------------------
bool CHTML::OnStartRequestInternal( const char *url, const char *target, const char *pchPostData, bool bIsRedirect )
{
	bool bAllow = true;
	DispatchEvent( HTMLStartRequest(), this, url, &bAllow );
	return bAllow;
}


//-----------------------------------------------------------------------------
// Purpose: show a popup window at this position and size
//-----------------------------------------------------------------------------
void CHTML::ShowPopup()
{

}


//-----------------------------------------------------------------------------
// Purpose: request to open a new tab inside an existing window
//-----------------------------------------------------------------------------
bool CHTML::OnOpenNewTab( const char *pchURL, bool bForeground )
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: popup a new html page at this location with this url
//-----------------------------------------------------------------------------
bool CHTML::OnPopupHTMLWindow( const char *pchURL, int x, int y, int wide, int tall )
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: we know the title of this page
//-----------------------------------------------------------------------------
void CHTML::SetHTMLTitle( const char *pchTitle )
{
	m_sHTMLTitle = pchTitle;
	DispatchEvent( HTMLTitle(), this, pchTitle );
}


//-----------------------------------------------------------------------------
// Purpose: we are loading a new image/script/etc in our html page
//-----------------------------------------------------------------------------
void CHTML::OnLoadingResource( const char *pchURL )
{
}


//-----------------------------------------------------------------------------
// Purpose: cef has new status for us
//-----------------------------------------------------------------------------
void CHTML::OnSetStatusText(const char *text)
{
	DispatchEvent( HTMLStatusText(), this, text );
}


//-----------------------------------------------------------------------------
// Purpose: html page wants this cursor now please
//-----------------------------------------------------------------------------
void CHTML::OnSetCursor( CursorCode cursor )
{

}


//-----------------------------------------------------------------------------
// Purpose: html control wants a file dialog shown
//-----------------------------------------------------------------------------
void CHTML::OnFileLoadDialog( const char *pchTitle, const char *pchInitialFile )
{
}


//-----------------------------------------------------------------------------
// Purpose: show a tooltip
//-----------------------------------------------------------------------------
void CHTML::OnShowToolTip( const char *pchText )
{
	if ( m_pTooltip )
	{
		OnHideToolTip();
	}

	m_pTooltip = new CTextTooltip( this, NULL );
	m_pTooltip->SetText( pchText );
	m_pTooltip->CalculatePosition();
	m_pTooltip->SetVisible( true );
}


//-----------------------------------------------------------------------------
// Purpose: given a showing tooltip update it
//-----------------------------------------------------------------------------
void CHTML::OnUpdateToolTip( const char *pchText )
{
	if ( m_pTooltip )
	{
		m_pTooltip->SetText( pchText );
	}
}


//-----------------------------------------------------------------------------
// Purpose: hide the showing tooltip
//-----------------------------------------------------------------------------
void CHTML::OnHideToolTip()
{
	if ( m_pTooltip )
	{
		m_pTooltip->SetVisible( false );
		delete m_pTooltip;
		m_pTooltip = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: when a Find()/FindNext()/FindPrevious() call is made a callback to tell you details of the find
//-----------------------------------------------------------------------------
void CHTML::OnSearchResults( int iActiveMatch, int nResults )
{

}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: response to a create request, cef made the browser object and is ready to load
//-----------------------------------------------------------------------------
void CHTML::BrowserReady( const CMsgBrowserReady *pCmdIn )
{
	m_bReady = true;
	m_nHTMLPageWide = -1;
	m_nHTMLPageTall = -1;

	if ( !m_sURLToLoad.IsEmpty() )
	{
		if ( m_sURLPostData.IsEmpty() )
			OpenURL( m_sURLToLoad.String() );
		else
			PostURL( m_sURLToLoad.String(), m_sURLPostData.String() );
	}

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( m_scrollHorizontal.m_nPageSize, m_scrollVertical.m_nPageSize, false, flLeft, flTop, flRight, flBottom );


	// make it fill the control minus any padding we have
	SetBrowserSize( GetActualLayoutWidth() - flLeft - flRight, GetActualLayoutHeight() - flTop - flBottom );
	m_ScrollLeft.Reset();
	m_ScrollUp.Reset();

	const ILocalizationString *pchTitle = UILocalize()->PchFindToken( NULL,  "#cef_error_title", k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None );
	const ILocalizationString *pchHeader = UILocalize()->PchFindToken(  NULL,  "#cef_error_header", k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None );
	const ILocalizationString *pchDetailCacheMiss = UILocalize()->PchFindToken( NULL, "#cef_cachemiss", k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None );
	const ILocalizationString *pchDetailBadUURL = UILocalize()->PchFindToken( NULL, "#cef_badurl", k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None );
	const ILocalizationString *pchDetailConnectionProblem = UILocalize()->PchFindToken( NULL, "#cef_connectionproblem", k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None );
	const ILocalizationString *pchDetailProxyProblem = UILocalize()->PchFindToken( NULL, "#cef_proxyconnectionproblem", k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None );
	const ILocalizationString *pchDetailUnknown = UILocalize()->PchFindToken( NULL, "#cef_unknown", k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None );

	// tell it utf8 loc strings to use
	{
		CHTMLProtoBufMsg<CMsgBrowserErrorStrings> cmd( eHTMLCommands_BrowserErrorStrings );
		cmd.Body().set_title( pchTitle->String() );
		cmd.Body().set_header( pchHeader->String() );
		cmd.Body().set_cache_miss( pchDetailCacheMiss->String() );
		cmd.Body().set_bad_url( pchDetailBadUURL->String() );
		cmd.Body().set_connection_problem( pchDetailConnectionProblem->String() );
		cmd.Body().set_proxy_problem( pchDetailProxyProblem->String() );
		cmd.Body().set_unknown( pchDetailUnknown->String() );
		DISPATCH_MESSAGE( eHTMLCommands_BrowserErrorStrings );
	}

	pchTitle->Release();
	pchHeader->Release();
	pchDetailCacheMiss->Release();
	pchDetailBadUURL->Release();
	pchDetailConnectionProblem->Release();
	pchDetailProxyProblem->Release();
	pchDetailUnknown->Release();

	m_bConfigureYouTubeHTML5OptIn = true;
	GetCookiesForURL( pchYouTubeCookieURL ); // make sure they are opted into the youtube html5 beta
	UIEngine()->AccessHTMLController()->SetWebCookie(  pchVimeoCookieURL, "html_player", "1", "/" );

	if ( m_bEmbedded && GetActualUIScaleY() != 1.0f )
	{
		CHTMLProtoBufMsg<CMsgScalePageToValue> cmd( eHTMLCommands_SetPageScale );
		cmd.Body().set_scale( GetActualUIScaleY() );
		cmd.Body().set_x( -1 );
		cmd.Body().set_y( -1 );
		DISPATCH_MESSAGE( eHTMLCommands_SetPageScale );
	}

}
#endif


//-----------------------------------------------------------------------------
// Purpose: check to see if we need to add scroll buffers to the edge of the texture, because the page is bigger than the screen in a dimension
//-----------------------------------------------------------------------------
void CHTML::ResizeBrowserTextureIfNeeded()
{
	// we get invalidated a lot, so cache our last size we told cef and only update when it changes
	if ( ( m_nHTMLPageWide != m_nWindowWide || m_nHTMLPageTall != m_nWindowTall ) && m_nWindowWide > 0 && m_nWindowTall > 0 )
	{
		m_nHTMLPageTall = m_nWindowTall;
		m_nHTMLPageWide = m_nWindowWide;
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->SetSize( m_HTMLBrowser, MAX( 0, m_nWindowWide ), MAX( 0, m_nWindowTall ) );
		}
#else
		CHTMLProtoBufMsg<CMsgBrowserSize> cmd( eHTMLCommands_BrowserSize );
		cmd.Body().set_width( MAX( 0, m_nWindowWide ) );
		cmd.Body().set_height( MAX( 0, m_nWindowTall ) );
		DISPATCH_MESSAGE( eHTMLCommands_BrowserSize );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: tell the browser to be this many pixels in side
//-----------------------------------------------------------------------------
void CHTML::SetBrowserSize( int wide, int tall )
{
	m_nWindowWide = wide; 
	m_nWindowTall = tall;

	ResizeBrowserTextureIfNeeded();

	CUILength width( wide, CUILength::k_EUILengthLength );
	CUILength height( tall, CUILength::k_EUILengthLength );
	
	width.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
	height.ScaleLengthValue( 1.0f / GetActualUIScaleY() );

	m_pTexurePanel->SetSize( width, height );
	m_pTexurePanel->InvalidateSizeAndPosition();
}


#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
#define CHECK_THIS_BROWSER( pCallback ) if ( pCallback->unBrowserHandle != m_HTMLBrowser ) { return; }

void CHTML::OnHTMLNeedsPaint( HTML_NeedsPaint_t *pHTML_NeedsPaint )
{
	CHECK_THIS_BROWSER( pHTML_NeedsPaint );
	if ( m_sCurrentURL.IsEmpty() || m_bSuppressTextureLoads )
	{
		return;
	}

	const byte *pRGBA = (const byte *)pHTML_NeedsPaint->pBGRA;

	Assert( pRGBA );
	uint nRGBAWide = pHTML_NeedsPaint->unWide;
	uint nRGBATall = pHTML_NeedsPaint->unTall;
	m_scrollHorizontal.m_nWebScroll = pHTML_NeedsPaint->unScrollX;
	m_scrollVertical.m_nWebScroll = pHTML_NeedsPaint->unScrollY;


	// see if we need to re-create the texture
	if ( !m_pDoubleBufferedTexturePending && (!m_pDoubleBufferedTexture || (m_pDoubleBufferedTexture->GetTextureWidth() != nRGBAWide
		|| m_pDoubleBufferedTexture->GetTextureHeight() != nRGBATall)) )
	{
		DbgVerify( AccessRenderDevice()->BCreateDoubleBufferedTexture( &m_pDoubleBufferedTexturePending, nRGBAWide,
			nRGBATall, nRGBAWide,
			k_EFormatBGR8, k_EAlphaChannelType_Normal, false ) );
		UIEngine()->DispatchEventAsync( 0.0f, AddStyle::MakeEvent( this, "HTMLContentLoaded" ) );

		m_nTextureWide = nRGBAWide;
		m_nTextureTall = nRGBATall;
	}

	// if we wanted a new texture and it has finished being constructed then flop to it
	if ( m_pDoubleBufferedTexturePending && m_pDoubleBufferedTexturePending->BIsReady() )
	{
		m_pDoubleBufferedTexture.SafeRelease();
		m_pDoubleBufferedTexture = m_pDoubleBufferedTexturePending;
		m_pDoubleBufferedTexturePending = nullptr;
		m_nTextureSerial = -1;
	}

	// On some platforms (Linux) the texture is created asychronously because of OpenGL threading issues;
	// Don't upload the texture until it is ready.
	if ( !m_pDoubleBufferedTexturePending && m_pDoubleBufferedTexture && m_pDoubleBufferedTexture->BIsReady() )
	{
		// we had a pending texture to upload, we are about to draw so lets do it now
		m_nTextureSerial = m_pDoubleBufferedTexture->UpdateTextureData( (void*)pRGBA );
	}

	// if we needed to capture a thumbnail lets do it now
	/*if ( m_bCaptureThumbNailThisFrame )
	{
		AUTO_LOCK( m_mutexScreenShot );
		m_bufScreenshotTexture.Put( pRGBA, nRGBAWide*nRGBATall * 4 );
		int nThumbnailWidth = k_nThumbNailWide;
		int nThumbnailHeight = k_nThumbNailTall;
		BResizeImageRGBA( m_bufScreenshotTexture, nRGBAWide, nRGBATall, nThumbnailWidth, nThumbnailHeight );
		m_bCaptureThumbNailThisFrame = false;

		UIEngine()->DispatchEventAsync( 0.0f, HTMLScreenShotCaptured::MakeEvent( this, nThumbnailWidth, nThumbnailHeight ) );
	}*/
}


void CHTML::OnHTMLStartRequest( HTML_StartRequest_t *pHTML_StartRequest )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	int index = m_vecDenyNewBrowserWindows.Find( pHTML_StartRequest->unBrowserHandle );
	if ( index != m_vecDenyNewBrowserWindows.InvalidIndex() )
	{
#if HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER
		if ( g_pHtmlSteamWebSurfaceGlobalCallbacksListener )
		{
			g_pHtmlSteamWebSurfaceGlobalCallbacksListener->m_rbKnownBrowserCallbackHandlers.Remove( pHTML_StartRequest->unBrowserHandle );
		}
#endif

		m_vecDenyNewBrowserWindows.Remove( index );
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->AllowStartRequest( pHTML_StartRequest->unBrowserHandle, false );
			SteamHTMLSurface()->RemoveBrowser( pHTML_StartRequest->unBrowserHandle );
		}
		return;
	}
#endif

	CHECK_THIS_BROWSER( pHTML_StartRequest );

	bool bAllow = OnStartRequestInternal( pHTML_StartRequest->pchURL, pHTML_StartRequest->pchTarget, pHTML_StartRequest->pchPostData, pHTML_StartRequest->bIsRedirect );
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->AllowStartRequest( m_HTMLBrowser, bAllow );
	}

	m_bIsSecure = m_bIsCertError = m_bIsEVCert = false;
	m_sCertName = "";

	bool bSameURL = m_sCurrentURL == pHTML_StartRequest->pchURL;
	if ( bAllow && V_strlen(pHTML_StartRequest->pchTarget) == 0 )
	{
		m_sCurrentURL = pHTML_StartRequest->pchURL;
	}
	if ( bAllow && !bSameURL )
	{
		SetHorizontalScroll( 0 );
		SetVerticalScroll( 0 );
		m_ScrollLeft.m_flLastScrollTime = 0.0f;
		m_ScrollUp.m_flLastScrollTime = 0.0f;
		m_bMousePanningActive = false;
		m_evtFocus.Reset();
	}
}

void CHTML::OnHTMLCloseBrowser( HTML_CloseBrowser_t *pHTML_CloseBrowser )
{
	CHECK_THIS_BROWSER( pHTML_CloseBrowser );
	DispatchEventAsync( HTMLCloseWindow(), this );
}

void CHTML::OnHTMLURLChanged( HTML_URLChanged_t *pHTML_URLChanged )
{
	CHECK_THIS_BROWSER( pHTML_URLChanged );
	if ( pHTML_URLChanged->bNewNavigation )
	{
		OnURLChanged( pHTML_URLChanged->pchURL, pHTML_URLChanged->pchPostData, pHTML_URLChanged->bIsRedirect );
		DispatchEvent( HTMLURLChanged(), this, pHTML_URLChanged->pchURL, pHTML_URLChanged->pchPageTitle );
	}

	DispatchEvent( HTMLLoadPage(), this, pHTML_URLChanged->pchURL );


}
void CHTML::OnHTMLFinishedRequest( HTML_FinishedRequest_t *pHTML_FinishedRequest )
{
	CHECK_THIS_BROWSER( pHTML_FinishedRequest );
	if ( m_bEmbedded && GetActualUIScaleY() != 1.0f )
	{
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->SetPageScaleFactor( m_HTMLBrowser, GetActualUIScaleY(), 0, 0 );
		}
	}

	const char *pPageTitle = pHTML_FinishedRequest->pchPageTitle[0] ? pHTML_FinishedRequest->pchPageTitle : m_sHTMLTitle.Get();
	OnFinishRequest( pHTML_FinishedRequest->pchURL, pPageTitle );
	DispatchEvent( HTMLFinishRequest(), this, pHTML_FinishedRequest->pchURL, pPageTitle );
}

void CHTML::OnHTMLOpenLinkInNewTab( HTML_OpenLinkInNewTab_t *pHTML_OpenLinkInNewTab )
{
	CHECK_THIS_BROWSER( pHTML_OpenLinkInNewTab );
	DispatchEventAsync( 0.0f, HTMLOpenLinkInNewTab(), this, pHTML_OpenLinkInNewTab->pchURL );

}

void CHTML::OnHTMLChangedTitle( HTML_ChangedTitle_t *pHTML_ChangedTitle )
{
	CHECK_THIS_BROWSER( pHTML_ChangedTitle );
	SetHTMLTitle( pHTML_ChangedTitle->pchTitle );
}

void CHTML::OnHTMLSearchResults( HTML_SearchResults_t *pHTML_SearchResults )
{
	CHECK_THIS_BROWSER( pHTML_SearchResults );
	OnSearchResults( pHTML_SearchResults->unCurrentMatch, pHTML_SearchResults->unResults );

}

void CHTML::OnHTMLCanGoBackAndForward( HTML_CanGoBackAndForward_t *pHTML_CanGoBackAndForward )
{
	CHECK_THIS_BROWSER( pHTML_CanGoBackAndForward );
	m_bCanGoBack = pHTML_CanGoBackAndForward->bCanGoBack;
	m_bCanGoForward = pHTML_CanGoBackAndForward->bCanGoForward;
	DispatchEvent( HTMLBackForwardState(), this, m_bCanGoBack, m_bCanGoForward );
}

void CHTML::OnHTMLHorizontalScroll( HTML_HorizontalScroll_t *pHTML_HorizontalScroll )
{
	CHECK_THIS_BROWSER( pHTML_HorizontalScroll );
	ScrollData_t scrollDataNew;
	scrollDataNew = m_scrollHorizontal;
	scrollDataNew.m_nPageSize = pHTML_HorizontalScroll->unPageSize;
	scrollDataNew.m_nMaxScroll = pHTML_HorizontalScroll->unScrollMax;
	scrollDataNew.m_bVisible = pHTML_HorizontalScroll->bVisible;
	scrollDataNew.m_nScroll = pHTML_HorizontalScroll->unScrollCurrent;
	if ( scrollDataNew != m_scrollHorizontal )
	{
		m_scrollHorizontal = scrollDataNew;
		ResizeBrowserTextureIfNeeded();
		InvalidateSizeAndPosition();
	}
}



void CHTML::OnHTMLVerticalScroll( HTML_VerticalScroll_t *pHTML_VerticalScroll )
{
	CHECK_THIS_BROWSER( pHTML_VerticalScroll );
	ScrollData_t scrollDataNew;
	scrollDataNew = m_scrollVertical;
	scrollDataNew.m_nPageSize = pHTML_VerticalScroll->unPageSize;
	scrollDataNew.m_nMaxScroll = pHTML_VerticalScroll->unScrollMax;
	scrollDataNew.m_bVisible = pHTML_VerticalScroll->bVisible;
	scrollDataNew.m_nScroll = pHTML_VerticalScroll->unScrollCurrent;

	if ( scrollDataNew != m_scrollVertical )
	{
		m_scrollVertical = scrollDataNew;
		ResizeBrowserTextureIfNeeded();
		InvalidateSizeAndPosition();
	}
}

void CHTML::OnHTMLJSAlert( HTML_JSAlert_t *pHTML_JSAlert )
{
	CHECK_THIS_BROWSER( pHTML_JSAlert );
	DispatchEvent( HTMLJSAlert(), this, (const char *)pHTML_JSAlert->pchMessage );
#if HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER
	// the global callback dismisses it, nothing we have to do here
#else
	DismissJSDialog( false );
#endif
}


void CHTML::OnHTMLJSConfirm( HTML_JSConfirm_t *pHTML_JSConfirm )
{
	CHECK_THIS_BROWSER( pHTML_JSConfirm );
	DispatchEvent( HTMLJSConfirm(), this, (const char *)pHTML_JSConfirm->pchMessage );
#if HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER
	// the global callback dismisses it, nothing we have to do here
#else
	DismissJSDialog( false );
#endif
}

void CHTML::OnHTMLFileOpenDialog( HTML_FileOpenDialog_t *pHTML_FileOpenDialog )
{
	CHECK_THIS_BROWSER( pHTML_FileOpenDialog );

#if HTML_STEAM_WEB_SURFACE_GLOBAL_CALLBACKS_LISTENER
	// the global callback dismisses it, nothing we have to do here
	return;
#endif

#if defined( PANORAMA_PUBLIC_STEAM_SDK )
	AssertMsg( false, "Implement file open dialog" );
#elif defined( SOURCE2_PANORAMA ) 
	Assert( !m_pFileOpenDialog.Get() );

	m_pFileOpenDialog = new CFileOpenDialog( GetParentWindow(), "HTMLFileDialog", FOD_OPEN );
	m_pFileOpenDialog->AddFilter( "*.*", "#UI_File_Filter_Any", true );
#else
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->FileLoadDialogResponse( m_HTMLBrowser, NULL );
	}
#endif
}

bool CHTML::OnFileOpenDialogFilesSelected( const CPanelPtr< IUIPanel > &ptrPanel, const char *pszFiles )
{
	if ( !m_pFileOpenDialog.Get() || ptrPanel->ClientPtr( ) != m_pFileOpenDialog.Get( ) )
		return true;

	CUtlVector<CUtlString> vecSelectedFiles;
	V_SplitString( pszFiles, ";", vecSelectedFiles );

	if ( vecSelectedFiles.Count() > 0 )
	{
		const char **ppFileNames = (const char**)stackalloc( ( vecSelectedFiles.Count() + 1 ) * sizeof( const char* ) );
		for ( int i = 0; i < vecSelectedFiles.Count(); i++ )
		{
			ppFileNames[ i ] = vecSelectedFiles[ i ].Get();
		}
		ppFileNames[ vecSelectedFiles.Count() ] = nullptr;

		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->FileLoadDialogResponse( m_HTMLBrowser, ppFileNames );
		}
	}
	else
	{
		if ( SteamHTMLSurface() )
		{
			SteamHTMLSurface()->FileLoadDialogResponse( m_HTMLBrowser, NULL );
		}
	}

	 m_pFileOpenDialog.Clear();
	 return true;
}

void CHTML::OnHTMLNewWindow( HTML_NewWindow_t *pHTML_NewWindow )
{
	CHECK_THIS_BROWSER( pHTML_NewWindow );

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	m_vecDenyNewBrowserWindows.AddToTail( pHTML_NewWindow->unNewWindow_BrowserHandle );
#else
	Assert( !"Needs impl" );
	//Assert( !m_pPopupChild );
	// needs to be sync so we can return its popup child below!
	//DispatchEvent( HTMLOpenPopupTab(), this, this );
#endif
}

void CHTML::OnHTMLSetCursor( HTML_SetCursor_t *pHTML_SetCursor )
{
	CHECK_THIS_BROWSER( pHTML_SetCursor );
	OnSetCursor( (CursorCode)pHTML_SetCursor->eMouseCursor );
}


void CHTML::OnHTMLStatusText( HTML_StatusText_t *pHTML_StatusText )
{
	CHECK_THIS_BROWSER( pHTML_StatusText );
	OnSetStatusText( pHTML_StatusText->pchMsg );
}

void CHTML::OnHTMLShowToolTip( HTML_ShowToolTip_t *pHTML_ShowToolTip )
{
	CHECK_THIS_BROWSER( pHTML_ShowToolTip );
	OnShowToolTip( pHTML_ShowToolTip->pchMsg );
}

void CHTML::OnHTMLUpdateToolTip( HTML_UpdateToolTip_t *pHTML_UpdateToolTip )
{
	CHECK_THIS_BROWSER( pHTML_UpdateToolTip );
	OnUpdateToolTip( pHTML_UpdateToolTip->pchMsg );
}

void CHTML::OnHTMLHideToolTip( HTML_HideToolTip_t *pHTML_HideToolTip )
{
	CHECK_THIS_BROWSER( pHTML_HideToolTip );
	OnHideToolTip();
}

#else

//-----------------------------------------------------------------------------
// Purpose: called when cef has made a new set of triple-buffered paint buffers
//-----------------------------------------------------------------------------
void CHTML::BrowserSetSharedPaintBuffers( const CMsgSetSharedPaintBuffers *pCmd )
{
	m_SharedPaintBuffer.BOpen( pCmd->wide(), pCmd->tall(), pCmd->handle() );
}

//-----------------------------------------------------------------------------
// Purpose: called when cef thinks we should repaint
//-----------------------------------------------------------------------------
void CHTML::BrowserNeedsPaint( const CMsgNeedsPaint *pCmd )
{
	if ( m_sCurrentURL.IsEmpty() || ( m_PageLoadCount != pCmd->pageserial() && pCmd->pageserial() != 0 ) || m_bSuppressTextureLoads )
	{
		return;
	}

	// In panorama we currently ignore damage regions. full redraw every time.
	int a, b, c, d;
	const byte *pRGBA = m_SharedPaintBuffer.LockForRead( &a, &b, &c, &d );
	if ( !pRGBA )
		return;
	
	Assert( pRGBA );
	uint nRGBAWide = m_SharedPaintBuffer.GetWidth();
	uint nRGBATall = m_SharedPaintBuffer.GetHeight();
	m_scrollHorizontal.m_nWebScroll = pCmd->scrollx();
	m_scrollVertical.m_nWebScroll = pCmd->scrolly();

	// see if we need to re-create the texture
	if ( !m_pDoubleBufferedTexturePending && ( !m_pDoubleBufferedTexture
			|| m_pDoubleBufferedTexture->GetTextureWidth() != nRGBAWide 
			|| m_pDoubleBufferedTexture->GetTextureHeight() != nRGBATall ) )
	{
		DbgVerify( AccessRenderEngine()->BCreateDoubleBufferedTexture( &m_pDoubleBufferedTexturePending,
			nRGBAWide, nRGBATall, nRGBAWide,
			k_EFormatBGR8, k_EAlphaChannelType_Normal, false ) );
		UIEngine()->DispatchEventAsync( 0.0f, AddStyle::MakeEvent( this, "HTMLContentLoaded" ) );

		m_nTextureWide = nRGBAWide;
		m_nTextureTall = nRGBATall;
	}

	// if we wanted a new texture and it has finished being constructed then flop to it
	if ( m_pDoubleBufferedTexturePending && m_pDoubleBufferedTexturePending->BIsReady() )
	{
		m_pDoubleBufferedTexture.SafeRelease();
		m_pDoubleBufferedTexture = m_pDoubleBufferedTexturePending;
		m_pDoubleBufferedTexturePending = NULL;
		m_nTextureSerial = -1;
	}

	// On some platforms (Linux) the texture is created asychronously because of OpenGL threading issues;
	// Don't upload the texture until it is ready.
	if ( !m_pDoubleBufferedTexturePending && m_pDoubleBufferedTexture->BIsReady() )
	{
		// we had a pending texture to upload, we are about to draw so lets do it now
		m_nTextureSerial = m_pDoubleBufferedTexture->UpdateTextureData( (void*)pRGBA );
	}
	else
	{
		// make sure we get a repaint once the texture is ready
		UIEngine()->DispatchEventAsync( 0.0f, HTMLRequestRepaint::MakeEvent( this ) );
	}

	// if we needed to capture a thumbnail lets do it now
	if ( m_bCaptureThumbNailThisFrame )
	{
		AUTO_LOCK( m_mutexScreenShot );
		m_bufScreenshotTexture.Put( pRGBA, nRGBAWide*nRGBATall*4 );
		int nThumbnailWidth = k_nThumbNailWide;
		int nThumbnailHeight = k_nThumbNailTall;
		BResizeImageRGBA( m_bufScreenshotTexture, nRGBAWide, nRGBATall, nThumbnailWidth, nThumbnailHeight );
		m_bCaptureThumbNailThisFrame = false;

		UIEngine()->DispatchEventAsync( 0.0f, HTMLScreenShotCaptured::MakeEvent( this, nThumbnailWidth, nThumbnailHeight ) );
	}

	m_SharedPaintBuffer.UnlockForRead();
}


//-----------------------------------------------------------------------------
// Purpose: can we upload our paints off the main thread?
//-----------------------------------------------------------------------------
bool CHTML::BSupportsOffMainThreadPaints()
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: threaded notification that the main thread has paints to process.
//-----------------------------------------------------------------------------
void CHTML::ThreadNotifyPendingPaints()
{
	// Unreached because we return true from BSupportsOffMainThreadPaints
}


//-----------------------------------------------------------------------------
// Purpose: called when cef starts loading a url
//-----------------------------------------------------------------------------
void CHTML::BrowserStartRequest( const CMsgStartRequest *pCmdIn )
{
	bool bAllow = OnStartRequestInternal( pCmdIn->url().c_str(), pCmdIn->target().c_str(), pCmdIn->postdata().c_str(), pCmdIn->bisredirect() );
	CHTMLProtoBufMsg<CMsgStartRequestResponse> cmd( eHTMLCommands_StartRequestResponse );
	cmd.Body().set_ballow( bAllow );
	DISPATCH_MESSAGE( eHTMLCommands_StartRequestResponse );

	m_bIsSecure = m_bIsCertError = 	m_bIsEVCert = false;
	m_sCertName = "";

	bool bSameURL = m_sCurrentURL == pCmdIn->url().c_str();
	if ( bAllow && pCmdIn->target().length() == 0 )
	{
		m_sCurrentURL = pCmdIn->url().c_str();
	}
	if ( bAllow && !bSameURL )
	{
		SetHorizontalScroll( 0 );
		SetVerticalScroll( 0 );
		m_ScrollLeft.m_flLastScrollTime = 0.0f;
		m_ScrollUp.m_flLastScrollTime = 0.0f;
		m_bMousePanningActive = false;
		m_evtFocus.Reset();
	}
}


//-----------------------------------------------------------------------------
// Purpose: url we are on has changed, loading a new page
//-----------------------------------------------------------------------------
void CHTML::BrowserURLChanged( const CMsgURLChanged *pCmd )
{
	if ( pCmd->bnewnavigation() )
	{
		OnURLChanged( pCmd->url().c_str(), pCmd->postdata().c_str(), pCmd->bisredirect() );
		DispatchEvent( HTMLURLChanged(), this, pCmd->url().c_str(), pCmd->pagetitle().c_str() );
	}
	
	DispatchEvent( HTMLLoadPage(), this, pCmd->url().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: finished loading a new page
//-----------------------------------------------------------------------------
void CHTML::BrowserFinishedRequest( const CMsgFinishedRequest *pCmd )
{
	OnFinishRequest( pCmd->url().c_str(), pCmd->pagetitle().c_str() );

	DispatchEvent( HTMLFinishRequest(), this, pCmd->url().c_str(), pCmd->pagetitle().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: main html doc has loaded
//-----------------------------------------------------------------------------
void CHTML::BrowserLoadedRequest(const CMsgLoadedRequest *pCmd)
{
	CUtlMap < CUtlString, CUtlString > mapHeaders;
	SetDefLessFunc(mapHeaders);
	for (int i = 0; i < pCmd->headers_size(); i++)
	{
		const CHTMLHeader &header = pCmd->headers(i);
		mapHeaders.Insert(header.key().c_str(), header.value().c_str());
	}

	OnPageLoaded( pCmd->url().c_str(), pCmd->pagetitle().c_str(), mapHeaders );
}


//-----------------------------------------------------------------------------
// Purpose: security details on the loaded page
//-----------------------------------------------------------------------------
void CHTML::BrowserPageSecurity(const CMsgPageSecurity *pCmd)
{
	DispatchEvent(HTMLSecurityStatus(), this, pCmd->url().c_str(), pCmd->security_info().bissecure(), pCmd->security_info().bhascerterror(), pCmd->security_info().bisevcert(), pCmd->security_info().certname().c_str());

	m_bIsSecure = pCmd->security_info().bissecure();
	m_bIsCertError = pCmd->security_info().bhascerterror();
	m_bIsEVCert = pCmd->security_info().bisevcert();
	m_sCertName = pCmd->security_info().certname().c_str();
}


//-----------------------------------------------------------------------------
// Purpose: show a popup at this position and size
//-----------------------------------------------------------------------------
void CHTML::BrowserShowPopup( const CMsgShowPopup *pCmd )
{
	m_bPopupVisible = true;
	ShowPopup();
}


//-----------------------------------------------------------------------------
// Purpose: done with the popup
//-----------------------------------------------------------------------------
void CHTML::BrowserHidePopup( const CMsgHidePopup *pCmd )
{
	m_bPopupVisible = false;
	HidePopup();
	m_pDoubleBufferedTextureComboBox.SafeRelease();
}


//-----------------------------------------------------------------------------
// Purpose: position and size of combo box to show
//-----------------------------------------------------------------------------
void CHTML::BrowserSizePopup(const CMsgSizePopup *pCmd) 
{
	m_nPopupX = pCmd->x();
	m_nPopupY = pCmd->y();
	m_nPopupWide = pCmd->wide();
	m_nPopupTall = pCmd->tall();
} 


//-----------------------------------------------------------------------------
// Purpose: texture data for the combo
//-----------------------------------------------------------------------------
void CHTML::BrowserComboNeedsPaint(const CMsgComboNeedsPaint *pCmd)
{
	const byte *pRGBA = NULL;

	if (pCmd->has_rgba())
		pRGBA = (const unsigned char *)pCmd->rgba();

	CCrossProcessSharedMemory textureSharedMemory;

	if (!pRGBA && pCmd->has_shared_memory_handle())
	{
		// dupe the shared memory handle into our own handle so we free it later
		textureSharedMemory.BInit((void *)(uintp)pCmd->shared_memory_handle(), NULL, pCmd->shared_memory_size(), true );
		if (!textureSharedMemory.BValid())
		{
			return;
		}

		pRGBA = (const unsigned char *)textureSharedMemory.Base();
	}

	Assert(pRGBA);

	if ( !m_pDoubleBufferedTextureComboBox || m_pDoubleBufferedTextureComboBox->GetTextureWidth() != pCmd->combobox_wide() || m_pDoubleBufferedTextureComboBox->GetTextureHeight() != pCmd->combobox_tall() )
	{
		m_pDoubleBufferedTextureComboBox.SafeRelease();
		DbgVerify(AccessRenderEngine()->BCreateDoubleBufferedTexture(&m_pDoubleBufferedTextureComboBox, pCmd->combobox_wide(),
			pCmd->combobox_tall(), pCmd->combobox_wide(), k_EFormatBGR8, k_EAlphaChannelType_Normal, true));
		m_nTextureSerialCombo = -1;
	}

#ifdef DEBUG
	// Test code to let you jpeg dump a texture from CEF
	bool bCapture = false;
	if (bCapture)
	{
		CUtlBuffer bufRGB;

		bufRGB.Put( pRGBA, pCmd->combobox_wide()*pCmd->combobox_tall() * 4);
		if (!BConvertRGBAToRGB(bufRGB, pCmd->combobox_wide(), pCmd->combobox_tall()))
			return;

		// input format is actually BGRA so now swizzle to rgb
		byte *pBGR = (byte *)bufRGB.Base();
		for (uint32 i = 0; i < pCmd->combobox_tall(); i++)
		{
			for (uint32 j = 0; j < pCmd->combobox_wide(); j++)
			{
				char cR = pBGR[0];
				pBGR[0] = pBGR[2];
				pBGR[2] = cR;
				pBGR += 3;
			}
		}

		if (!ConvertRGBToJpeg("page.jpg", 95, pCmd->combobox_wide(), pCmd->combobox_tall(), bufRGB))
			return;
	}
#endif

	m_ComboTexture.EnsureCapacity( pCmd->combobox_wide() * pCmd->combobox_tall() * 4 );
	V_memcpy( m_ComboTexture.Base(), pRGBA, pCmd->combobox_wide() * pCmd->combobox_tall() * 4 );
	m_ComboTexture.SeekPut( CUtlBuffer::SEEK_HEAD, pCmd->combobox_wide() * pCmd->combobox_tall() * 4 );
}


//-----------------------------------------------------------------------------
// Purpose: open a new tabbed window
//-----------------------------------------------------------------------------
void CHTML::BrowserOpenNewTab( const CMsgOpenNewTab *pCmd )
{
	DispatchEventAsync( 0.0f, HTMLOpenLinkInNewTab(), this, pCmd->url().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: popup a new browser page
//-----------------------------------------------------------------------------
IHTMLResponses *CHTML::BrowserPopupHTMLWindow( const CMsgPopupHTMLWindow *pCmd )
{
	m_pPopupChild = NULL;
	// needs to be sync so we can return its popup child below!
	DispatchEvent(  HTMLOpenPopupTab(), this, this, pCmd->url().c_str() );
	Assert( m_pPopupChild ); // should be set now
	return m_pPopupChild;
}


//-----------------------------------------------------------------------------
// Purpose: set the title for this page
//-----------------------------------------------------------------------------
void CHTML::BrowserSetHTMLTitle( const CMsgSetHTMLTitle *pCmd )
{
	SetHTMLTitle( pCmd->title().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: loading a resource (image/script/etc) in this page
//-----------------------------------------------------------------------------
void CHTML::BrowserLoadingResource( const CMsgLoadingResource *pCmd )
{
	OnLoadingResource( pCmd->url().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: new status details from cef
//-----------------------------------------------------------------------------
void CHTML::BrowserStatusText( const CMsgStatusText *pCmd )
{
	OnSetStatusText( pCmd->text().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: use this cursor on the page
//-----------------------------------------------------------------------------
void CHTML::BrowserSetCursor( const CMsgSetCursor *pCmd )
{
	OnSetCursor( (CursorCode)pCmd->cursor() );
}


//-----------------------------------------------------------------------------
// Purpose: show this toolip
//-----------------------------------------------------------------------------
void CHTML::BrowserShowToolTip( const CMsgShowToolTip *pCmd )
{
	OnShowToolTip( pCmd->text().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: done showing the tooltip
//-----------------------------------------------------------------------------
void CHTML::BrowserUpdateToolTip( const CMsgUpdateToolTip *pCmd )
{
	OnUpdateToolTip( pCmd->text().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: hide an active tooltip
//-----------------------------------------------------------------------------
void CHTML::BrowserHideToolTip( const CMsgHideToolTip *pCmd )
{
	OnHideToolTip();

}


//-----------------------------------------------------------------------------
// Purpose: we have answers from a find
//-----------------------------------------------------------------------------
void CHTML::BrowserSearchResults( const CMsgSearchResults *pCmd )
{
	OnSearchResults( pCmd->activematch(), pCmd->results() );
}


//-----------------------------------------------------------------------------
// Purpose: when javascript wants to close this page for it
//-----------------------------------------------------------------------------
void CHTML::BrowserClose( const CMsgClose *pCmd )
{
	// async dispatch so the html control can be safely deleted by this event
	DispatchEventAsync( HTMLCloseWindow(), this );
}
#endif


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: scroll sizing details from cef
//-----------------------------------------------------------------------------
void CHTML::BrowserHorizontalScrollBarSizeResponse( const CMsgHorizontalScrollBarSizeResponse *pCmd )
{
	ScrollData_t scrollDataNew;
	scrollDataNew = m_scrollHorizontal;
	scrollDataNew.m_nPageSize	= pCmd->page_size();
	scrollDataNew.m_nMaxScroll	= pCmd->scroll_max();
	scrollDataNew.m_bVisible	= pCmd->visible(); 
	scrollDataNew.m_nScroll		= pCmd->scroll();

	// Plat_OutputDebugString( "BrowserHorizontalScrollBarSizeResponse: m_nPageSize:%d, m_nMaxScroll:%d, m_nScroll:%d\n", scrollDataNew.m_nPageSize, scrollDataNew.m_nMaxScroll, scrollDataNew.m_nScroll );
		
	if ( scrollDataNew != m_scrollHorizontal )
	{
		m_scrollHorizontal = scrollDataNew;
		ResizeBrowserTextureIfNeeded();
		InvalidateSizeAndPosition();
	}
}


//-----------------------------------------------------------------------------
// Purpose: scroll sizing details from cef
//-----------------------------------------------------------------------------
void CHTML::BrowserVerticalScrollBarSizeResponse( const CMsgVerticalScrollBarSizeResponse *pCmd )
{
	ScrollData_t scrollDataNew;
	scrollDataNew = m_scrollVertical;

	scrollDataNew.m_nPageSize = pCmd->page_size();
	scrollDataNew.m_nMaxScroll = pCmd->scroll_max();
	scrollDataNew.m_bVisible = pCmd->visible();
	scrollDataNew.m_nScroll = pCmd->scroll();

	// Plat_OutputDebugString( "BrowserVerticalScrollBarSizeResponse: m_nPageSize:%d, m_nMaxScroll:%d, m_nScroll:%d\n", scrollDataNew.m_nPageSize, scrollDataNew.m_nMaxScroll, scrollDataNew.m_nScroll );

	if ( scrollDataNew != m_scrollVertical )
	{
		m_scrollVertical = scrollDataNew;
		ResizeBrowserTextureIfNeeded();
		InvalidateSizeAndPosition();
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: temporary helper funciton to pop an OS file dialog, until we design a tenfoot one
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
bool OpenOSFileOpenDialog( const char *pchTitle, const char *pchInitialFile, const char *pchFilters, char *pchFileSelected, int cchFileSelectedMax )
{
	return false;
}
#elif defined( WIN32 )
bool OpenOSFileOpenDialog( const char *pchTitle, const char *pchInitialFile, const char *pchFilters, char *pchFileSelected, int cchFileSelectedMax )
{
	bool bSuccess = false;
	wchar_t wchFilters[1024] = { 0 };
	wchar_t wchTitle[1024] = { 0 };

	wchar_t* pwchFileSelected = new wchar_t[cchFileSelectedMax];

	if ( pchTitle )
		V_UTF8ToUnicode( pchTitle, wchTitle, sizeof(wchTitle) );

	if ( pchFilters )
		V_UTF8ToUnicode( pchFilters, wchFilters, sizeof(wchFilters) );

	OPENFILENAMEW openfilename = { 0 };
	openfilename.lStructSize = sizeof(openfilename);
	openfilename.hwndOwner = NULL;
	openfilename.lpstrFilter = wchFilters;
	openfilename.lpstrCustomFilter = NULL;
	openfilename.lpstrFile = pwchFileSelected;
	openfilename.nMaxFile = cchFileSelectedMax;
	openfilename.Flags = OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;
	openfilename.lpstrTitle = wchTitle;

	pwchFileSelected[0] = 0;
	if ( pchInitialFile )
	{
		V_UTF8ToUnicode( pchInitialFile, pwchFileSelected, cchFileSelectedMax*sizeof(wchar_t) );
	}

	// GetOpenFileName() will return true if a file is selected, or false if there is an error or no file selected
	// if the user doesn't select a file, we still return true, since the dialog was still opened
	if ( ::GetOpenFileNameW( &openfilename ) || CommDlgExtendedError() == 0 )
	{
		// user picked a file, tanslate back to UTF8
		V_UnicodeToUTF8( pwchFileSelected, pchFileSelected, cchFileSelectedMax );
		bSuccess = true;
	}

	delete [] pwchFileSelected;

	// return result

	return bSuccess;
}
#elif defined( OSX )
bool OpenOSFileOpenDialog( const char *pchTitle, const char *pchInitialFile, const char *pchFilters, char *pchFileSelected, int cchFileSelectedMax )
{
	return false;
}
#elif defined( LINUX )
bool OpenOSFileOpenDialog( const char *pchTitle, const char *pchInitialFile, const char *pchFilters, char *pchFileSelected, int cchFileSelectedMax )
{
	return false;
}
#else
#error "File OS dialog please"
#endif


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: ask the user to pick a file
//-----------------------------------------------------------------------------
void CHTML::BrowserFileLoadDialog( const CMsgFileLoadDialog *pCmd )
{
//	OnFileLoadDialog( pCmdIn->title().c_str(), pCmdIn->initialfile().c_str() );

	CHTMLProtoBufMsg<CMsgFileLoadDialogResponse> cmd( eHTMLCommands_FileLoadDialogResponse );

	// try and use the OS-specific file dialog until we get a tenfoot one made
	char rgchFileName[MAX_UNICODE_PATH_IN_UTF8];
	if ( OpenOSFileOpenDialog( pCmd->title().c_str(), pCmd->initialfile().c_str(), NULL, rgchFileName, sizeof(rgchFileName) ) )
	{
		cmd.Body().add_files( rgchFileName );
	}
	
	DISPATCH_MESSAGE( eHTMLCommands_FileLoadDialogResponse );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: called when the gamepad is used
//-----------------------------------------------------------------------------
bool CHTML::OnGamepadInput( GamePadCode eGamePadCode )
{
	if( BIsGamePadCodeForController( eGamePadCode, k_EActiveControllerType_XInput ) )
	{
		m_flGamePadInputTime = Plat_FloatTime();
	}

	return false; // let the event bubble to parents
}


//-----------------------------------------------------------------------------
// Purpose: queue a screenshot to be taken of the current page
//-----------------------------------------------------------------------------
void CHTML::SaveCurrentPageToJPEG( const char *pchFileName, int nWide, int nTall )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	Assert( !"No impl yet" );
#else
	CHTMLProtoBufMsg<CMsgSavePageToJPEG> cmd( eHTMLCommands_SavePageToJPEG );

	cmd.Body().set_url( m_sCurrentURL );
	cmd.Body().set_filename( pchFileName );
	cmd.Body().set_width( nWide );
	cmd.Body().set_height( nTall );

	DISPATCH_MESSAGE( eHTMLCommands_SavePageToJPEG );
#endif
}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: a screenshot was taken
//-----------------------------------------------------------------------------
void CHTML::BrowserSavePageToJPEGResponse( const CMsgSavePageToJPEGResponse *pCmd )
{
	DispatchEvent( HTMLScreenShotTaken(), this, pCmd->url().c_str(), pCmd->filename().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: cef wants us to pop an alert dialog to the user
//-----------------------------------------------------------------------------
void CHTML::BrowserJSAlert( const CMsgJSAlert *pCmd )
{
	bool bHandled = false;
	DispatchEvent( HTMLJSAlert(), this, (const char *)pCmd->message().c_str(), &bHandled );
	if ( !bHandled )
		DismissJSDialog( false );
}


//-----------------------------------------------------------------------------
// Purpose: cef wants us to pop an confirmation dialog to the user
//-----------------------------------------------------------------------------
void CHTML::BrowserJSConfirm( const CMsgJSConfirm *pCmd )
{
	bool bHandled = false;
	DispatchEvent( HTMLJSConfirm(), this, pCmd->message().c_str(), &bHandled );
	if ( !bHandled )
		DismissJSDialog( false );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: done with a JS confirm dialog
//-----------------------------------------------------------------------------
void CHTML::DismissJSDialog( bool bRetVal )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->JSDialogResponse( m_HTMLBrowser, bRetVal );
	}
#else
	CHTMLProtoBufMsg<CMsgJSDialogResponse> cmd( eHTMLCommands_JSDialogResponse );
	cmd.Body().set_result( bRetVal );
	DISPATCH_MESSAGE( eHTMLCommands_JSDialogResponse );
#endif
}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: cache of the forward/back button state
//-----------------------------------------------------------------------------
void CHTML::BrowserCanGoBackandForward( const CMsgCanGoBackAndForward *pCmd )
{
	m_bCanGoBack = pCmd->bgoback();
	m_bCanGoForward = pCmd->bgoforward();
	DispatchEvent( HTMLBackForwardState(), this, m_bCanGoBack, m_bCanGoForward );
}


//-----------------------------------------------------------------------------
// Purpose: open a steam specific url
//-----------------------------------------------------------------------------
void CHTML::BrowserOpenSteamURL( const CMsgOpenSteamURL *pCmd )
{
	DispatchEventAsync( ExecuteSteamURL(), NULL, pCmd->url().c_str() );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: return the current number of paints since last reset, and then reset,
//			 used by the main frame loop to rate limit html thinking if nothing is visible
//-----------------------------------------------------------------------------
uint32 CHTML::GetAndResetPaintCounter()
{ 
	uint32 nRet = sm_PaintCount;
	sm_PaintCount = 0;
	return nRet;
}


//-----------------------------------------------------------------------------
// Purpose: send any queued html messages we have
//-----------------------------------------------------------------------------
void CHTML::SendPendingHTMLMessages()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->SetKeyFocus( m_HTMLBrowser, m_bLastKeyFocus );
	}
#else
	FOR_EACH_VEC( m_vecPendingMessages, i )
	{
		m_vecPendingMessages[ i ]->m_iBrowser = m_iBrowser;
		((int *)m_vecPendingMessages[i]->m_Buffer.Base())[2] = m_iBrowser; // format is 3 ints, size, type and browser, update browser index 
		UIEngine()->AccessHTMLController()->PushCommand( m_vecPendingMessages[ i ] );
	}
	m_vecPendingMessages.RemoveAll();

	CHTMLProtoBufMsg<CMsgSetFocus> cmd( eHTMLCommands_SetFocus );
	cmd.Body().set_focus( m_bLastKeyFocus );
	DISPATCH_MESSAGE( eHTMLCommands_SetFocus );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: if we have a html texture uploaded, delete it
//-----------------------------------------------------------------------------
void CHTML::ReleaseTextureMemory( bool bSuppressTextureLoads )
{
	if ( m_pDoubleBufferedTexture ) 
	{
		// clear the current html image
		m_pDoubleBufferedTexture.SafeRelease();
		m_nTextureSerial = -1;
		m_pTexurePanel->SetRepaint( k_EPanelRepaintFull );
	}
	m_bSuppressTextureLoads = bSuppressTextureLoads;
}


//-----------------------------------------------------------------------------
// Purpose: async callback to request new texture data
//-----------------------------------------------------------------------------
bool CHTML::OnHTMLRequestRepaint( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel )
{
	if ( ptrPanel.Get() == UIPanel() )
	{
		RefreshTextureMemory();
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: ask cef to send us a new rgba image
//-----------------------------------------------------------------------------
void CHTML::RefreshTextureMemory()
{
	m_bSuppressTextureLoads = false;

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->Reload( m_HTMLBrowser );
	}
#else
	CHTMLProtoBufMsg<CMsgFullRepaint> cmd( eHTMLCommands_FullRepaint );
	DISPATCH_MESSAGE( eHTMLCommands_FullRepaint );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: capture a thumbnail of this control
//-----------------------------------------------------------------------------
void CHTML::CaptureThumbNailImage( CPanel2D *pEventTarget, int iUserData ) 
{ 
	m_bCaptureThumbNailThisFrame = true; 
	m_nCaptureUserData = iUserData;
	m_pCaptureEventTarget = pEventTarget;
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	CHTMLProtoBufMsg<CMsgFullRepaint> cmd( eHTMLCommands_FullRepaint );
	DISPATCH_MESSAGE( eHTMLCommands_FullRepaint );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: browser texture thread has a screenshot for us
//-----------------------------------------------------------------------------
bool CHTML::OnHTMLScreenShotCaptured( const panorama::CPanelPtr< IUIPanel > &ptrPanel, int nThumbNailWidth, int nThumbNailHeight )
{
	if ( m_pCaptureEventTarget )
	{
		AUTO_LOCK( m_mutexScreenShot );
		DispatchEvent( HMTLThumbNailImage(), m_pCaptureEventTarget, m_nCaptureUserData, &m_bufScreenshotTexture, nThumbNailWidth, nThumbNailHeight );
		m_pCaptureEventTarget = NULL;
		m_bufScreenshotTexture.Purge();
	}
	return true;
}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: CEF wants us to go full screen
//-----------------------------------------------------------------------------
void CHTML::BrowserRequestFullScreen( const CMsgRequestFullScreen *pCmd )
{
	CHTMLProtoBufMsg<CMsgRequestFullScreenResponse> cmd( eHTMLCommands_RequestFullScreenResponse );
	cmd.Body().set_ballow( true );
	m_bFullScreen = true;
	DISPATCH_MESSAGE( eHTMLCommands_RequestFullScreenResponse );

	DispatchEvent( HTMLFullScreen(), this, m_bFullScreen );

	SetHorizontalScroll( 0 );
	SetVerticalScroll( 0 );
	m_ScrollLeft.m_flLastScrollTime = 0.0f;
	m_ScrollUp.m_flLastScrollTime = 0.0f;
	m_scrollVertical.m_nPageSize = m_nWindowTall;
	m_scrollHorizontal.m_nPageSize = m_nWindowWide;

	ResizeBrowserTextureIfNeeded();
}


//-----------------------------------------------------------------------------
// Purpose: CEF is leaving full screen
//-----------------------------------------------------------------------------
void CHTML::BrowserExitFullScreen( const CMsgExitFullScreen *pCmd )
{
	m_bFullScreen = false;
	DispatchEvent( HTMLFullScreen(), this, m_bFullScreen );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: done with fullscreen, tell CEF to stop
//-----------------------------------------------------------------------------
void CHTML::ExitFullScreen()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	Assert( !"Not supported" );
#else
	if ( m_bFullScreen )
	{
		{
			CHTMLProtoBufMsg<CMsgExitFullScreen> cmd( eHTMLCommands_ExitFullScreen );
			DISPATCH_MESSAGE( eHTMLCommands_ExitFullScreen );
		}
		{
			CHTMLProtoBufMsg<CMsgCloseFullScreenFlashIfOpen> cmd( eHTMLCommands_CloseFullScreenFlashIfOpen );
			DISPATCH_MESSAGE( eHTMLCommands_CloseFullScreenFlashIfOpen );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: tell cef to pause any flash video if some is playing in fullscreen
//-----------------------------------------------------------------------------
void CHTML::PauseFlashVideoIfVisible()
{
	if ( m_bFullScreen )
	{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
		Assert( !"Not supported" );
#else
		CHTMLProtoBufMsg<CMsgPauseFullScreenFlashMovieIfOpen> cmd( eHTMLCommands_PauseFullScreenFlashMovieIfOpen );
		DISPATCH_MESSAGE( eHTMLCommands_PauseFullScreenFlashMovieIfOpen );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: run this javascript snippet on the current page
//-----------------------------------------------------------------------------
void CHTML::ExecuteJavaScript( const char *pchScript )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( SteamHTMLSurface() )
	{
		SteamHTMLSurface()->ExecuteJavascript( m_HTMLBrowser, pchScript );
	}
#else
	CHTMLProtoBufMsg<CMsgExecuteJavaScript> cmd( eHTMLCommands_ExecuteJavaScript );
	cmd.Body().set_script( pchScript );
	DISPATCH_MESSAGE( eHTMLCommands_ExecuteJavaScript );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: run this javascript snippet on the current page
//-----------------------------------------------------------------------------
void CHTML::GetCookiesForURL( const char *pchURL )
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	Assert( !"No impl" );
#else
	CHTMLProtoBufMsg<CMsgGetCookiesForURL> cmd( eHTMLCommands_GetCookiesForURL );
	cmd.Body().set_url( pchURL );
	DISPATCH_MESSAGE( eHTMLCommands_GetCookiesForURL );
#endif
}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: run this javascript snippet on the current page
//-----------------------------------------------------------------------------
void CHTML::BrowserGetCookiesForURLResponse( const CMsgGetCookiesForURLResponse *pCmd )
{
	if ( m_bConfigureYouTubeHTML5OptIn )
	{
		if ( pCmd->url() == pchYouTubeCookieURL )
		{
			m_bConfigureYouTubeHTML5OptIn = false;
			for ( int i = 0; i < pCmd->cookies_size(); i++ )
			{
				const CCookie &cookie = pCmd->cookies(i);
				if ( cookie.name() == "PREF" )
				{
					const char *pchf2Key = V_strstr( cookie.value().c_str(), "f2=" );
					if ( pchf2Key && V_strlen(pchf2Key) > 10 )
					{
						if ( !V_strnicmp( pchf2Key + 3, "40000000", 8 ) )
							return; // already opted in
						else
						{
							AssertMsg1( false, "Already have the f2 key set, how do I manipulate it? (%s)", pchf2Key );
							return;
						}
					}
					else
					{
						CUtlString sValue = cookie.value().c_str();
						sValue += "&f2=40000000";
						UIEngine()->AccessHTMLController()->SetWebCookie(  pchYouTubeCookieURL, "PREF", sValue, "/" );
						return;
					}
				}
			}

			UIEngine()->AccessHTMLController()->SetWebCookie(  pchYouTubeCookieURL, "PREF", "f2=40000000", "/" );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: new dom element got focus
//-----------------------------------------------------------------------------
void CHTML::BrowserNodeGotFocus( const CMsgNodeHasFocus *pCmd )
{
	m_evtFocus.m_bInput = pCmd->binput();
	m_evtFocus.m_sSearchLabel = pCmd->searchbuttontext().c_str();
	m_evtFocus.m_bInputHasMultiplePeers = pCmd->bhasmultipleinputs();
	m_evtFocus.m_sInputType = pCmd->input_type().c_str();
	if ( m_evtFocus.m_sInputType == "checkbox" || m_evtFocus.m_sInputType == "button" || m_evtFocus.m_sInputType == "radio" )
		m_evtFocus.m_bInput = false;
	m_evtFocus.m_sName = pCmd->name().c_str();
	m_bFocusEventSentForClick = true;
	m_evtFocus.m_bFocusedElementChanged = true;

	DispatchEvent( HTMLFormHasFocus(), this, m_evtFocus, m_sCurrentURL.String() ); // bring the daisy wheel back up
}
#endif


//-----------------------------------------------------------------------------
// Purpose: zoom to the html node that currently has key focus
//-----------------------------------------------------------------------------
void CHTML::ZoomPageToFocusedElement()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	Assert( !"Needs impl" );
#else
	CHTMLProtoBufMsg<CMsgZoomToFocusedElement> cmd( eHTMLCommands_ZoomToCurrentlyFocusedNode );
	DISPATCH_MESSAGE( eHTMLCommands_ZoomToCurrentlyFocusedNode );
#endif
	RefreshTextureMemory();
}


//-----------------------------------------------------------------------------
// Purpose: if true don't allow links to be activated, essentially making the page static
//-----------------------------------------------------------------------------
void CHTML::SetIgnoreCursor( bool bState )
{
	m_bIgnoreCursor = bState;
}


//-----------------------------------------------------------------------------
// Purpose: check if we didn't see key focus change on the web page, and if we had input pop daisywheel
//-----------------------------------------------------------------------------
bool CHTML::OnHTMLFormFocusPending( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( m_evtFocus.m_bInput && m_LinkAtPos.m_bInput && !m_bFocusEventSentForClick )
	{
		// didn't send a focus event from a click but input had focus, synthesize an event here
		m_bFocusEventSentForClick = true;
		m_evtFocus.m_bFocusedElementChanged = false;
		DispatchEvent( HTMLFormHasFocus(), this, m_evtFocus, m_sCurrentURL.String() ); // bring the daisy wheel back up
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: ask the html control for the string value in the focused dom node
//-----------------------------------------------------------------------------
void CHTML::RequestFocusedNodeValue()
{
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	Assert( !"Needs impl" );
#else
	CHTMLProtoBufMsg<CMsgFocusedNodeText> cmd( eHTMLCommands_GetFocusedNodeValue );
	DISPATCH_MESSAGE( eHTMLCommands_GetFocusedNodeValue );
#endif
}


#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
//-----------------------------------------------------------------------------
// Purpose: got an answer back about the value of text in the focused node
//-----------------------------------------------------------------------------
void CHTML::BrowserFocusedNodeValueResponse( const CMsgFocusedNodeTextResponse *pCmd )
{
	DispatchEvent( HTMLFocusedNodeValue(), this, pCmd->value().c_str() );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: we got focus, lets start tracking the mouse
//-----------------------------------------------------------------------------
bool CHTML::OnInputFocusSet( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel  )
{
	// ask to always get OnMouseMoved calls so we track correctly when the reticle is up
	if ( GetMouseCanActivate() != k_EMouseCanActivateUnfocused )
		SetMouseTracking( true );

	UpdateCursorBehaviourOnFocusChange( true );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: we lost focus, stop tracking the mouse
//-----------------------------------------------------------------------------
bool CHTML::OnInputFocusLost( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel )
{
	// stop tracking if we aren't in view
	if ( GetMouseCanActivate() != k_EMouseCanActivateUnfocused )
		SetMouseTracking( false );

	UpdateCursorBehaviourOnFocusChange( false );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTML::BCanScrollInDirection( EHTMLScrollDirection eDirection ) const
{
	if ( eDirection == kHTMLScrollDirection_Up )
		return m_scrollVertical.m_nScroll > 0;

	if ( eDirection == kHTMLScrollDirection_Down )
		return m_scrollVertical.m_nScroll < m_scrollVertical.m_nMaxScroll;

	if ( eDirection == kHTMLScrollDirection_Left )
		return m_scrollHorizontal.m_nScroll > 0;

	if ( eDirection == kHTMLScrollDirection_Right )
		return m_scrollHorizontal.m_nScroll < m_scrollHorizontal.m_nMaxScroll;

	UNREACHABLE();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHTML::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	BaseClass::GetDebugPropertyInfo( pvecProperties );

	if ( !m_sCurrentURL.IsEmpty() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t();
		pProperty->m_strName = "url";
		pProperty->m_strValue = m_sCurrentURL;

		// We might be too long to display, so if necessary copy the string contents and do a safe UTF8 truncation.
		enum { kMaxUTF8CodePointLength = 100 };
		if ( V_UnicodeLength( m_sCurrentURL ) > kMaxUTF8CodePointLength )
		{
			V_UnicodeTruncate( pProperty->m_strValue.Access(), kMaxUTF8CodePointLength );
			pProperty->m_strValue.Append( "..." );
		}

		pvecProperties->AddToTail( pProperty );
	}
}


//-----------------------------------------------------------------------------
// Purpose: it's possible that we have an order of events like "get an event, feed
//			it to our child panel manually, which doesn't want it, so it feeds it
//			to its parent panel, which is us". If this happens, we want to detect
//			it and early-abort the from-child event.
//-----------------------------------------------------------------------------
class CReentrantScope
{
public:
	CReentrantScope( bool& bVarRef )
		: m_bVarRef( bVarRef )
	{
		Assert( !m_bVarRef );
		m_bVarRef = true;
	}

	~CReentrantScope()
	{
		Assert( m_bVarRef );
		m_bVarRef = false;
	}

private:
	bool& m_bVarRef;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CHTMLSimpleNavigationWrapper::CHTMLSimpleNavigationWrapper( CPanel2D *pParent, const char *pchPanelID )
	: BaseClass( pParent, pchPanelID )
{
	m_bInEventProcessing = false;
	m_pHTML = nullptr;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTMLSimpleNavigationWrapper::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol k_symWrappedHTMLID( "wrappedhtmlid" );

	if ( symName == k_symWrappedHTMLID )
	{
		Assert( !m_pHTML );
		Assert( !m_symWrappedHTMLID.IsValid() );

		m_symWrappedHTMLID = pchValue;
		return true;
	}
	else
	{
		return BaseClass::BSetProperty( symName, pchValue );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTMLSimpleNavigationWrapper::OnGamePadDown( const GamePadData_t &code )
{
	if ( m_bInEventProcessing )
		return false;

	CReentrantScope scope( m_bInEventProcessing );

	switch ( code.m_GamePadCode )
	{
	case XK_STICK1_UP:
	case XK_STICK1_DOWN:
	case XK_STICK1_LEFT:
	case XK_STICK1_RIGHT:
	case STEAM_LEFTSTICK_UP:
	case STEAM_LEFTSTICK_DOWN:
	case STEAM_LEFTSTICK_LEFT:
	case STEAM_LEFTSTICK_RIGHT:
		// We want to do special processing for this input.
		break;

	case XK_STICK2_UP:
	case XK_STICK2_DOWN:
	case XK_STICK2_LEFT:
	case XK_STICK2_RIGHT:
		// We want to silently swallow this input.
		return true;

	case XK_BUTTON_A:
	case STEAM_BUTTON_A:
		// Override the controller activation buttons to move focus down. Would be great to generalize this for
		// any generic event but there's one use for this wrapper in the entire UI right now and this is it.
		DispatchEvent( MoveDown(), this, 0 );
		return true;

	default:
		// We didn't handle this input.
		return false;
	}

	EnsureHTMLPanelReference();

	// Which direction are we trying to move in? Can we scroll farther in that direction? If we can, feed the
	// input to our HTML panel. If we can't, have it escape up the rest of the stack.
	const CHTML::EHTMLScrollDirection eDirection = (code.m_GamePadCode == XK_STICK1_UP || code.m_GamePadCode == STEAM_LEFTSTICK_UP)
												 ? CHTML::kHTMLScrollDirection_Up
												 : (code.m_GamePadCode == XK_STICK1_DOWN || code.m_GamePadCode == STEAM_LEFTSTICK_DOWN)
												 ? CHTML::kHTMLScrollDirection_Down
												 : (code.m_GamePadCode == XK_STICK1_LEFT || code.m_GamePadCode == STEAM_LEFTSTICK_RIGHT)
												 ? CHTML::kHTMLScrollDirection_Left
												 : CHTML::kHTMLScrollDirection_Right;

	if ( m_pHTML->BCanScrollInDirection( eDirection ) )
	{
		// Always swallow the input for these presses if we haven't reached our scroll edges yet,
		// regardless of whether we think we processed it or not.
		(void)m_pHTML->OnGamePadDownImpl( code );
		return true;
	}
	
	return BaseClass::OnGamePadDown( code );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTMLSimpleNavigationWrapper::OnGamePadAnalog( const GamePadData_t &code )
{
	if ( m_bInEventProcessing )
		return false;

	CReentrantScope scope( m_bInEventProcessing );

	EnsureHTMLPanelReference();

	if ( code.m_GamePadCode == XK_STICK1_ANALOG || code.m_GamePadCode == STEAM_LEFTSTICK_ANALOG )
	{
		m_pHTML->ProcessAnalogScroll( code.m_fValueX, code.m_fValueY, UIInputEngine()->GetDeadZoneValue( XK_STICK1_ANALOG ), false );
	}
	else if ( code.m_GamePadCode == XK_STICK2_ANALOG || code.m_GamePadCode == STEAM_LEFTPAD_ANALOG || code.m_GamePadCode == STEAM_RIGHTPAD_ANALOG )
	{
		// Swallow input from the touchpads and the right analog stick.
	}
	else
	{
		// Pass other analog inputs through to the underlying control.
		(void)m_pHTML->OnGamePadAnalogImpl( code );
	}

	return BaseClass::OnGamePadAnalog( code );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHTMLSimpleNavigationWrapper::OnMouseWheel( const MouseData_t &code )
{
	if ( m_bInEventProcessing )
		return false;

	CReentrantScope scope( m_bInEventProcessing );

	EnsureHTMLPanelReference();

	return m_pHTML->OnMouseWheel( code );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHTMLSimpleNavigationWrapper::EnsureHTMLPanelReference()
{
	if ( !m_pHTML )
	{
		m_pHTML = assert_cast<CHTML *>( FindChildTraverse( m_symWrappedHTMLID.String() ) );
		Assert( m_pHTML );
	}
}
